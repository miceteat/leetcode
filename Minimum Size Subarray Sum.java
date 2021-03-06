/*
Given an array of n positive integers and a positive integer s, find the minimal length of a subarray of which the sum ≥ s. If there isn't one, return 0 instead.

For example, given the array [2,3,1,2,4,3] and s = 7,
the subarray [4,3] has the minimal length under the problem constraint.
*/


/************My solution**************/
//O(n) time O(1) space
public class Solution {
    public int minSubArrayLen(int s, int[] nums) {
        int sum = 0;
        int min = Integer.MAX_VALUE;
        int j = 0;
        for(int i = 0; i < nums.length; i++) {
            sum += nums[i];
            while(sum >= s) {
                min = Math.min(min, i - j + 1);
                sum -= nums[j++];
            }
        }
        
        return min == Integer.MAX_VALUE? 0 : min;
        
    }
}