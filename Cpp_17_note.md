**C++17新方法**`nth_element`
```cpp
template <class RandomIt, class Compare>
void nth_element(RandomIt first, RandomIt nth, RandomIt last, Compare comp);
```
第 nth 位置的元素，就是如果完全排序后应该出现在该位置的元素
所有在 `[first, nth)` 的元素都 ≤（或根据 comp 不小于）第 nth 元素
所有在 `[nth, last)` 的元素都 ≥（或根据 comp 不大于）第 nth 元素

示例：寻找前k个高频元素（leetcode347）
```cpp
class Solution {
public:
    vector<int> topKFrequent(vector<int>& nums, int k) {
        unordered_map<int, int> freq;
        // 统计所有元素的频率
        for (int num : nums) {
            freq[num] ++;
        }
        // 使用vector存储频率对
        vector<pair<int, int>> vec(freq.begin(), freq.end());
        // 使用nth_element，按照频率排列
        nth_element(vec.begin(), vec.begin()+k-1, vec.end(), [](pair<int, int>&a, pair<int, int>&b) {
            return a.second > b.second;
        });
        vector<int> result;
        for (int i = 0; i < k; i++) {
            result.push_back(vec[i].first);
        }
        return result;
    }
};
```
此例中，前k个元素都是频率大于等于第k-1位置元素的，通过lamda表达式决定comp规则