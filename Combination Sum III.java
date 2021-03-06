/*
Find all possible combinations of k numbers that add up to a number n, given that only numbers from 1 to 9 can be used and each combination should be a unique set of numbers.

Ensure that numbers within the set are sorted in ascending order.


Example 1:

Input: k = 3, n = 7

Output:

[[1,2,4]]

Example 2:

Input: k = 3, n = 9

Output:

[[1,2,6], [1,3,5], [2,3,4]]
*/


/************My solution**************/
public class Solution {
    List<List<Integer>> res;
    List<Integer> list;
    public List<List<Integer>> combinationSum3(int k, int n) {
        res = new ArrayList<>();
        list = new ArrayList<>();
        
        if(k > n || n > 45)
            return res;
        
        helper(k, n, 1);
        return res;
    }
    
    void helper(int k , int sum, int begin) {
        if(k == 0 && sum == 0) {
            res.add(new ArrayList(list));
            return;
        }
        else if (k == 0 || sum == 0)
            return;
        
        for(int i = begin; i <= 9; i++) {
            if(sum - i < 0)
                break;
            list.add(i);
            helper(k - 1, sum - i, i + 1);
            list.remove(list.size() - 1);
        }
    }
}