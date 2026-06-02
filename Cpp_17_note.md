<style>
    p {
        text-indent: 2em; /* 首行缩进2字符 */
    }
</style>

###**C++17新方法**`nth_element`
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

###`std::unique_ptr::get()`
`get`方法用于从智能指针中获取原始裸指针。它不参与内存管理，不转移指针所有权，无法使用`delete`，返回的裸指针不会造成内存泄露。指针内存释放仍由`unique_ptr`析构时完成

```cpp
// ❌ unique_ptr 不能拷贝
Node node = root;  // 编译错误

// ❌ 会转移所有权
Node* node = root.release();  // root 失去所有权！

// ✅ get() 只查看，不转移
Node* node = root.get();  // 安全！
```