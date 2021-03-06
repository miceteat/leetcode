/* 
 * loop.c	- execution loop and the related helper functions
 *
 * sish		- a simple shell
 *
 * Members:
 *		- Gong Cheng,	gcheng2@stevens.edu
 *		- Maisi Li,	mli27@stevens.edu
 *
 * Usage:	- ./sish [-x][-c command]
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "loop.h"
#include "sish.h"

/* execution for the shell */
int
execute()
{
	/* e.g. "ls -l | cat -n" */
	char line[MAX_LINE_LEN];
	/* e.g. "ls -l", "cat -n" */
	char cmd[MAX_CMD][MAX_CMD_LEN];
	/* e.g. "ls", "-l" */
	char buf[MAX_ARGC][MAX_ARG_LEN];
	int status;
	/* background flag */
	int is_bg;
	char str[2];
	int i, n;
	/* processes to be waited */
	pid_t wpid[MAX_CMD];
	int cmd_num;
	COMMAND command;

	is_bg = 0;

	/* handle child processes exit signal and ctrl-c signal */
	if (signal(SIGCHLD, chldHandler) == SIG_ERR ||
		signal(SIGINT, intHandler) == SIG_ERR) {
		perror("Signal() error");
		return DEFAULT_STATUS;
	}

	if (cflag) {
		if ((n = syntax_check(ccmd, &is_bg)) == -1) {
			fprintf(stderr, "sish: %s: Syntax error\n", ccmd);
			return DEFAULT_STATUS;
		} else if (n == 0)
			return EXIT_SUCCESS;

		cmd_num = command_parser(ccmd, cmd);

		if (xflag) {
			trace(cmd, cmd_num);
		}

		/* exit and cd */
		if (cmd_num == 1) {
			command_init(&command);
			argument_parser(cmd[0], buf, &command);

			if (strcmp(command.arg[0], "exit") == 0) {
				if (exit_syntax_check(command.arg) == -1) {
					exit_usage();
					ret_val = EXIT_FAILURE;
					return ret_val;
				} else {
					return ret_val;
				}
			} else if (strcmp(command.arg[0], "cd") == 0) {
				if (cd_syntax_check(command.arg) == -1) {
					cd_usage();
					ret_val = EXIT_FAILURE;
					return ret_val;
				} 

				ret_val = cd_exec(command.arg);

				return ret_val;
			}
		}

		execute_command(cmd, wpid, cmd_num);

		if (!is_bg) {
			for (i = 0; i < cmd_num; i++) {
				if (waitpid(wpid[i], &status, 0) == -1) {
					if (errno == ECHILD) {
						errno = 0;
						continue;
					}
					fprintf(stderr, "%d: waitpid() error"
							": %s\n", wpid[i],
							strerror(errno));
					return ret_val;
				}
				ret_val = check_status(status);
			}
		}

		return ret_val;
	}

	for (;;) {
LBEGIN:		print();
		i = 0;
		while ((n = read(STDIN_FILENO, str, 1)) > 0) {
			str[1] = '\0';
			if (strcmp(str, "\n") == 0) {
				line[i] = '\0';
				break;
			}
			if (i >= MAX_LINE_LEN - 1) {
				fprintf(stderr, "sish: commands too long\n");
				ret_val = EXIT_FAILURE;
				goto LBEGIN;
			}
			line[i] = str[0];
			i++;
		}

		if (n < 0) {
			fprintf(stderr, "sish: read() error: %s\n",
					strerror(errno));
			ret_val = EXIT_FAILURE;
			continue;
		}

		if ((n = syntax_check(line, &is_bg)) == -1) {
			fprintf(stderr, "sish: %s: Syntax error\n", line);
			ret_val = DEFAULT_STATUS;
			continue;
		} else if (n == 0) {
			continue;
		}

		cmd_num = command_parser(line, cmd);

		if (xflag) {
			trace(cmd, cmd_num);
		}

		/* exit and cd */
		if (cmd_num == 1) {
			command_init(&command);
			argument_parser(cmd[0], buf, &command);

			if (strcmp(command.arg[0], "exit") == 0) {
				if (exit_syntax_check(command.arg) == -1) {
					exit_usage();
					ret_val = EXIT_FAILURE;
					continue;
				} else {
					break;
				}
			} else if (strcmp(command.arg[0], "cd") == 0) {
				if (cd_syntax_check(command.arg) == -1) {
					cd_usage();
					ret_val = EXIT_FAILURE;
					continue;
				} 

				ret_val = cd_exec(command.arg);

				continue;
			}
		}

		execute_command(cmd, wpid, cmd_num);

		if (!is_bg) {
			for (i = 0; i < cmd_num; i++) {
				if (waitpid(wpid[i], &status, 0) == -1) {
					if (errno == ECHILD) {
						errno = 0;
						continue;
					}
					fprintf(stderr, "%d: waitpid() error"
							": %s\n", wpid[i],
							strerror(errno));
					return ret_val;
				}
				ret_val = check_status(status);
			}
		}
	}

	return ret_val;
}

/* check the exit status */
static int
check_status(int status)
{
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	} else
		return DEFAULT_STATUS;
}

/* execution for commands */
static void
execute_command(char cmd[MAX_CMD][MAX_CMD_LEN], pid_t wpid[MAX_CMD], 
		int cmd_num)
{
	/* e.g. "ls", "-l" */
	char buf[MAX_ARGC][MAX_ARG_LEN];
	/* two pipes needed */
	int fds[2][2];
	int n;
	int fd;
	pid_t pid;
	/* 0: pipe1, 1: pipe2 */
	int pipe_flag;
	COMMAND command;

	n = 0;
	pipe_flag = 0;

	while (n < cmd_num) {
		command_init(&command);
		argument_parser(cmd[n], buf, &command);

		if (!pipe_flag) {	/* pipe1 */
			/* last command doesn't need to create a pipe */
			if(n != cmd_num - 1) {
				if (pipe(fds[0]) == -1) {
					fprintf(stderr, "%s: pipe() error: "
						"%s\n", command.arg[0], 
						strerror(errno));

					/* close read end of previous pipe */
					if (n != 0)
						close(fds[1][0]);

					while (n < cmd_num) {
						wpid[n] = -1;
						n++;
					}

					break;
				}
			}

			if ((pid = fork()) == -1){
				fprintf(stderr, "%s: fork() error: %s\n",
					command.arg[0], strerror(errno));

				close(fds[0][0]);
				close(fds[0][1]);

				/* close read end of previous pipe */
				if (n != 0)
					close(fds[1][0]);

				while (n < cmd_num) {
					wpid[n] = -1;
					n++;
				}

				break;
			} else if (pid == 0) {	/* child */
				/* 
				 * Close read end in child process.
				 * Last command doesn't create a pipe.
				 */
				if(n != cmd_num - 1)
					close(fds[0][0]);

				/* stdin redirection */
				if(strcmp(command.input, "") != 0) {
					if ((fd = open(command.input, O_RDONLY))
						== -1) {
						fprintf(stderr, "%s: %s: open"
							"() error: %s\n",
							command.arg[0],
							command.input,
							strerror(errno));

						exit(DEFAULT_STATUS);
					}

					if (dup2(fd, STDIN_FILENO) == -1) {
						fprintf(stderr, "%s: %s: dup2()"
							"error: %s\n",
							command.arg[0],
							command.input,
							strerror(errno));

						close(fd);
						exit(DEFAULT_STATUS);
					}

					close(fd);
				} else if(n != 0){
					if (dup2(fds[1][0], STDIN_FILENO) == 
							-1) {
						fprintf(stderr, "%s: dup2()"
							"error: %s\n",
							command.arg[0],
							strerror(errno));

						close(fds[1][0]);
						exit(DEFAULT_STATUS);
					}
				}

				/* close read end of previous pipe */
				if(n != 0)
					close(fds[1][0]);

				/* stdout redirection */
				if(strcmp(command.output,"")!=0) {
					if(command.append)
						fd = open(command.output,
							O_WRONLY | O_APPEND
							| O_CREAT, S_IWUSR |
							S_IRUSR);
					else
						fd = open(command.output,
							O_WRONLY | O_TRUNC
							| O_CREAT, S_IWUSR |
							S_IRUSR);

					if (fd == -1) {
						fprintf(stderr, "%s: %s: open"
							"() error: %s\n",
							command.arg[0],
							command.output,
							strerror(errno));

						exit(DEFAULT_STATUS);
					}

					if (dup2(fd, STDOUT_FILENO) == -1) {
						fprintf(stderr, "%s: %s: dup2()"
							"error: %s\n",
							command.arg[0],
							command.output,
							strerror(errno));

						close(fd);
						exit(DEFAULT_STATUS);
					}

					close(fd);
				} else if (n != cmd_num - 1){
					if (dup2(fds[0][1], STDOUT_FILENO) ==
							-1) {
						fprintf(stderr, "%s: dup2()"
							"error: %s\n",
							command.arg[0],
							strerror(errno));

						close(fds[0][1]);
						exit(DEFAULT_STATUS);
					}
				}

				if(n != cmd_num - 1)
					close(fds[0][1]);

				/* builtin commands */
				if (strcmp(command.arg[0], "exit") == 0) {
					if (exit_syntax_check(command.arg) == 
						-1) {
						exit_usage();
						exit(EXIT_FAILURE);
					} else
						exit(ret_val);
				} else if (strcmp(command.arg[0], "cd") == 0) {
					if (cd_syntax_check(command.arg) == 
						-1) {
						cd_usage();
						exit(EXIT_FAILURE);
					} else {
						exit(cd_exec(command.arg));
					}
				} else if (strcmp(command.arg[0], "echo") == 
						0) {
					echo_exec(command.arg);
					exit(EXIT_SUCCESS);
				}

				execvp(command.arg[0], command.arg);

				fprintf(stderr, "%s: command not found\n",
					command.arg[0]);
				exit(DEFAULT_STATUS);
			} else {	/* parent */
				wpid[n] = pid;

				if(n != cmd_num - 1)
					close(fds[0][1]);

				if(n != 0)
					close(fds[1][0]);
			}

			pipe_flag = 1;
		} else {	/* pipe2 */
			/* last command doesn't need to create a pipe */
			if(n != cmd_num - 1) {
				if (pipe(fds[1]) == -1) {
					fprintf(stderr, "%s: pipe() error: "
						"%s\n", command.arg[0], 
						strerror(errno));

					/* close read end of previous pipe */
					if (n != 0)
						close(fds[0][0]);

					while (n < cmd_num) {
						wpid[n] = -1;
						n++;
					}

					break;
				}
			}

			if ((pid = fork()) == -1){
				fprintf(stderr, "%s: fork() error: %s\n",
					command.arg[0], strerror(errno));

				close(fds[1][0]);
				close(fds[1][1]);

				/* close read end of previous pipe */
				if (n != 0)
					close(fds[0][0]);

				while (n < cmd_num) {
					wpid[n] = -1;
					n++;
				}

				break;
			} else if (pid == 0) {	/* child */
				/* 
				 * Close read end in child process.
				 * Last command doesn't create a pipe.
				 */
				if(n != cmd_num - 1)
					close(fds[1][0]);

				/* stdin redirection */
				if(strcmp(command.input, "") != 0) {
					if ((fd = open(command.input, O_RDONLY))
						== -1) {
						fprintf(stderr, "%s: %s: open"
							"() error: %s\n",
							command.arg[0],
							command.input,
							strerror(errno));

						exit(DEFAULT_STATUS);
					}

					if (dup2(fd, STDIN_FILENO) == -1) {
						fprintf(stderr, "%s: %s: dup2()"
							"error: %s\n",
							command.arg[0],
							command.input,
							strerror(errno));

						close(fd);
						exit(DEFAULT_STATUS);
					}

					close(fd);
				} else if(n != 0){
					if (dup2(fds[0][0], STDIN_FILENO) == 
							-1) {
						fprintf(stderr, "%s: dup2()"
							"error: %s\n",
							command.arg[0],
							strerror(errno));

						close(fds[0][0]);
						exit(DEFAULT_STATUS);
					}
				}

				/* close read end of previous pipe */
				if(n != 0)
					close(fds[0][0]);

				/* stdout redirection */
				if(strcmp(command.output,"")!=0) {
					if(command.append)
						fd = open(command.output,
							O_WRONLY | O_APPEND
							| O_CREAT, S_IWUSR |
							S_IRUSR);
					else
						fd = open(command.output,
							O_WRONLY | O_TRUNC
							| O_CREAT, S_IWUSR |
							S_IRUSR);

					if (fd == -1) {
						fprintf(stderr, "%s: %s: open"
							"() error: %s\n",
							command.arg[0],
							command.output,
							strerror(errno));

						exit(DEFAULT_STATUS);
					}

					if (dup2(fd, STDOUT_FILENO) == -1) {
						fprintf(stderr, "%s: %s: dup2()"
							"error: %s\n",
							command.arg[0],
							command.output,
							strerror(errno));

						close(fd);
						exit(DEFAULT_STATUS);
					}

					close(fd);
				} else if (n != cmd_num - 1){
					if (dup2(fds[1][1], STDOUT_FILENO) ==
							-1) {
						fprintf(stderr, "%s: dup2()"
							"error: %s\n",
							command.arg[0],
							strerror(errno));

						close(fds[1][1]);
						exit(DEFAULT_STATUS);
					}
				}

				if(n != cmd_num - 1)
					close(fds[1][1]);

				/* builtin commands */
				if (strcmp(command.arg[0], "exit") == 0) {
					if (exit_syntax_check(command.arg) == 
						-1) {
						exit_usage();
						exit(EXIT_FAILURE);
					} else
						exit(ret_val);
				} else if (strcmp(command.arg[0], "cd") == 0) {
					if (cd_syntax_check(command.arg) == 
						-1) {
						cd_usage();
						exit(EXIT_FAILURE);
					} else {
						exit(cd_exec(command.arg));
					}
				} else if (strcmp(command.arg[0], "echo") == 
						0) {
					echo_exec(command.arg);
					exit(EXIT_SUCCESS);
				}

				execvp(command.arg[0], command.arg);

				fprintf(stderr, "%s: command not found\n",
					command.arg[0]);
				exit(DEFAULT_STATUS);
			} else {	/* parent */
				wpid[n] = pid;

				if(n != cmd_num - 1)
					close(fds[1][1]);

				if(n != 0)
					close(fds[0][0]);
			}
			pipe_flag = 0;
		}
		n++;
	}
}

static void
trace(char cmd[MAX_CMD][MAX_CMD_LEN], int cmd_num)
{
	int i;
	COMMAND command;
	char buf[MAX_ARGC][MAX_ARG_LEN];
	int j;

	for (i = 0; i < cmd_num; i++) {
		command_init(&command);
		argument_parser(cmd[i], buf, &command);
		fprintf(stderr, "+ ");
		for (j = 0; command.arg[j] != NULL; j++)
			fprintf(stderr, "%s ", command.arg[j]);
		fprintf(stderr, "\n");
	}
}

static void
print()
{
	printf("sish$ ");
	fflush(stdout);
}

static void
intHandler()
{
	printf("\n");
	print();
}

static void
chldHandler()
{
	int status;
	while (waitpid(-1, &status, WNOHANG) > 0) {
		ret_val = check_status(status);
	}
}
