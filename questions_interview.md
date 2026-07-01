<style>
    p {
        text-indent: 2em; /* 首行缩进2字符 */d
    }
</style>

### STL中`push_back`和`emplace_back`的区别
**核心区别**

| 特性 | `push_back` | `emplace_back` |
|------|-------------|----------------|
| **参数** | 接受对象本身 | 接受构造函数的参数 |
| **构造次数** | 1次构造 + 1次拷贝/移动 | 1次直接构造 |
| **临时对象** | 可能产生临时对象 | 不产生临时对象 |
| **效率** | 较慢（可能多一次拷贝） | 更快（直接构造） |
| **适用版本** | C++98 起 | C++11 起 |

**基本用法对比**

```cpp
#include <vector>
#include <string>

struct Person {
    std::string name;
    int age;
    
    Person(const std::string& n, int a) : name(n), age(a) {
        std::cout << "构造: " << name << std::endl;
    }
    
    Person(const Person& other) : name(other.name), age(other.age) {
        std::cout << "拷贝构造: " << name << std::endl;
    }
    
    Person(Person&& other) noexcept : name(std::move(other.name)), age(other.age) {
        std::cout << "移动构造: " << name << std::endl;
    }
};
```

- 默认使用`emplace_back`，更灵活高效
- **不可拷贝类型**只能用`emplace_back`
- 初始化列表场景可以使用`push_back`
- `noexcept`声明函数不返回任何异常(比如不生成栈展开的额外代码)。移动语义本质为指针所有权转移，不涉及新的内存分配，因此一般不会抛出异常。`std::vector`扩容时，如果移动构造函数是`noexcept`，优先使用移动语义；如果不是则退化为拷贝语义。

| 类型 | 符号 | 含义 |
| :-- | :-- | :-- |
| 左值引用 | `T&` | 指向可寻址、有名字的变量 |
| 右值引用 | `T&&` | 指向即将销毁的临时对象 |
| 常引用 | `const T&` | 能绑定左或右值，但只能读 |

写一个类的复制拷贝函数(深拷贝问题)
```cpp
class MyClass{
private:
    char* data;
public:
// 构造函数
    MyClass(const char* str = "") : data(nullptr) {
        data = new char[strlen(str)+1];
        strcpy(data, str);
    }
// 拷贝构造
    MyClass(const MyClass& idx) : data(nullptr) {
        data = new char[strlen(idx.data)+1];
        strcpy(data, idx.data);
    }
// 重载赋值运算符
    MyClass& operator=(const MyClass& other) {
        if (this != &other) {
            delete[] data;
            data = new char[strlen(other.data)+1];
            strcpy(data, other.data);
        }
        return *this;
    }
// 析构
    ~MyClass(){
        delete[] data;
    }
/**
* MyClass的构造函数保证MyClass对象的data指向一个有效内存
* 这个内存至少为{'\0'}
* 但是在构造函数和拷贝构造中，将data初始化为nullptr
* 这样可以避免data为未定义的随机值（野指针）
* 如果new抛出异常，析构函数会执行`delete[] data`
* 由于此时data为野指针，将会发生未定义行为造成崩溃
*/
};
```
new和new[]会在内存中记录不同元数据，后者会在内存块头部记录数组大小（元素个数），delete[]会读取这个信息逐个调用析构函数；delete只会调用第一个元素的析构函数

### `map`和`unordered_map`的区别及使用场景
| 特性 | `std::map` | `std::unordered_map` |
|------|------------|----------------------|
| **底层实现** | 红黑树（平衡二叉搜索树） | 哈希表 |
| **元素顺序** | 按键自动排序（升序） | 无序 |
| **查找复杂度** | O(log n) | O(1) 平均，O(n) 最坏 |
| **插入复杂度** | O(log n) | O(1) 平均，O(n) 最坏 |
| **删除复杂度** | O(log n) | O(1) 平均，O(n) 最坏 |
| **空间占用** | 较小（每个节点多存指针） | 较大（需要哈希表数组） |
| **迭代器类型** | 双向迭代器 | 前向迭代器 |
| **内存布局** | 节点分散 | 桶连续 + 节点分散 |
| **适用版本** | C++98 起 | C++11 起 |

### map中的key按照自定义排序
`std::map`的模板定义：
```cpp
template<class Key, class T, class Compare = std::less<key>>
class map
```
其中`compare`就是key的排序比较器，默认形式为`std::less<key>`升序

**自定义方法**
1. 函数对象
   ```cpp
    #include <iostream>
    #include <map>
    #include <string>

    // 自定义比较器：按字符串长度降序（长 -> 短）
    struct CompareByLengthDesc {
        bool operator()(const std::string& a, const std::string& b) const {
            if (a.length() != b.length()) {
                return a.length() > b.length();  // 长度降序
            }
            return a < b;  // 长度相同时按字典序
        }
    };

    int main() {
        std::map<std::string, int, CompareByLengthDesc> myMap;
        
        myMap["apple"] = 1;
        myMap["banana"] = 2;
        myMap["cat"] = 3;
        myMap["dog"] = 4;
        
        for (const auto& [key, value] : myMap) {
            std::cout << key << ": " << value << std::endl;
        }
        // 输出顺序：banana (6), apple (5), cat (3), dog (3)
        // 注：cat 和 dog 长度相同，按字典序 cat < dog
        
        return 0;
    }
    ```
    在`return a < b`中，判断的是a、b的字典序，而非其值大小。`<`为`std::string`运算符，用途是逐个字符比较a和b的ASCII码

    `std::map`的第三个模板参数只作用于键，不涉及值
2. Lambda表达式
   ```cpp
   #include <iostream>
   #include <map>
   #include <string>

   int main() {
    // 定义lambda表达式
    auto cmp = [](const std::string& a, const std::string& b) {
        if (a.length() != b.length()) {
            return a.length() > b.length();  // 长度降序排列
        }
        return a < b;
    };
    // 使用decltype自动推导lambda类型，传入lambda对象
    std::map<std::string, int, decltype(cmp)> myMap(cmp);

    myMap["apple"] = 1;
    myMap["banana"] = 2;
    myMap["cat"] = 3;
    
    for (const auto& [key, value] : myMap) {
        std::cout << key << ": " << value << std::endl;
    }
    // 输出：banana: 2, apple: 1, cat: 3
    
    return 0;
   }
   ```
3. 函数指针
### 手撕memcpy
```cpp
# include <cstddef>     // std::size_t

void* my_memcpy(void* dest, const void* src, std::size_t count) {
    void* result = dest;
    // 判断无效情况
    if (dest == nullptr || src == nullptr || count = 0) {
        return result;
    }
    // 使用无符号字符（1字节）避免符号扩展问题
    unsigned char* d = static_cast<unsigned char*>(dest);
    const unsigned char* s = static_cast<const unsigned char*>(src);
    // 得到复制和源的低地址，判断是否存在风险区域
    if (d < s) {
        // 无风险
        for (std::size_t i = 0; i < count; i++) {
            d[i] = s[i];
        }
    } else {
        // 需从高位开始
        for (std::size_t i = count-1; i >=0; i--) {
            d[i] = s[i];
        }
    }
    return result;
}
```
`size_t`是C++中专门用于表示对象大小、数组长度的**无符号整数**类型，通常由`sizeof`返回。一般用于表示内存大小、数组索引、循环大小。在此例中，`size_t`用于表示内存中的字节数量。`sizeof`,`strlen`,`malloc`等内存相关标准库函数均采用`size_t`

### 谈谈STL
基于模板技术实现，提供大量可服用数据结构和算法
1. **容器**
用于储存数据

- 顺序容器：`vector`,`deque`,`list`,`forward_list`
- 关联容器：`set`,`map`,`multiset`,`multimap`
底层通常使用红黑树实现。自动有序。查找、插入、删除时间复杂度为O(logN)
- 无序关联容器：`unorderde_map`,`unordered_set`,`unordered_multimap`,`unordered_multiset`
底层使用哈希表。查找插入平均复杂度O(1)
2. 算法
```text
sort()
find()
count()
binary_search()
lower_bound()
upper_bound()
for_each()
remove()
reverse()
```
以上为常用通用算法
3. 迭代器
4. 仿函数
5. 适配器:对已有容器进行封装
典型用例
```text
stack
queue
priority_queue
```
6. 分配器：负责内存管理

### 哈希表及其原理

哈希表（Hash Table）是一种根据键（Key）直接访问数据的数据结构，其核心思想是通过哈希函数将键映射到数组中的某个位置，从而实现快速的查找、插入和删除操作。

哈希表主要由两部分组成：
1. 哈希函数（Hash Function）
2. 存储数据的桶（Bucket）数组

其工作过程如下：

当插入一个键值对时，首先通过哈希函数计算键对应的哈希值，然后根据哈希值确定元素应该存储的位置。例如：

查找时同样先计算哈希值，然后直接定位到对应桶，从而避免遍历整个容器，因此平均时间复杂度为 O(1)。

哈希冲突（Hash Collision）：不同的 Key 经过哈希计算后得到相同的位置。
```text
1 % 10 = 1
11 % 10 = 1
21 % 10 = 1
```
多个元素都会映射到同一个桶。

解决哈希冲突的方法：
1. 链地址法
每个桶维护一个链表或其他容器，发生冲突的元素挂接在同一个桶后面
```text
bucket[1]
   ↓
[1] -> [11] -> [21]
```
C++ STL 中的 unordered_map 和 unordered_set 通常采用这种思想实现。

2. 开放寻址法
发生冲突后不使用链表，而是在数组中继续寻找空闲位置。

### 介绍vector和list的区别

`vector` 和 `list` 都是 STL 中常用的容器，但它们的底层实现和适用场景有较大区别。

首先，`vector` 的底层是**动态数组**，而 `list` 的底层是**双向链表**。

1. 底层结构

#### vector

```text
+----+----+----+----+----+
| 1  | 2  | 3  | 4  | 5  |
+----+----+----+----+----+
```

特点：

* 内存连续存储
* 支持随机访问
* 可以通过下标直接访问元素

例如：

```cpp
vector<int> v{1,2,3};

cout << v[1];
```

访问时间复杂度为 O(1)。

---

#### list

```text
+-----+    +-----+    +-----+
|  1  |<-->|  2  |<-->|  3  |
+-----+    +-----+    +-----+
```

每个节点包含：

```cpp
prev + data + next
```

特点：

* 内存不连续
* 节点之间通过指针连接
* 不支持随机访问

例如：

```cpp
list<int> lst;
```

不能写：

```cpp
lst[2];
```

因为链表必须从头开始逐个遍历。

---

2. 随机访问能力

#### vector

由于内存连续：

```cpp
v[i];
```

底层实际上是：

```cpp
*(begin + i)
```

时间复杂度：

```text
O(1)
```

---

#### list

链表需要逐个节点查找：

```text
head -> node1 -> node2 -> node3 ...
```

时间复杂度：

```text
O(N)
```

因此：

* vector 支持随机访问迭代器
* list 仅支持双向迭代器

很多 STL 算法（如 sort、binary_search）更适合 vector。

---

3. 插入和删除

#### vector

尾部插入：

```cpp
v.push_back(x);
```

平均：

```text
O(1)
```

但如果容量不足发生扩容：

```text
申请新空间
↓
拷贝旧数据
↓
释放旧空间
```

代价较大。

中间插入：

```cpp
v.insert(pos, x);
```

需要移动后续元素：

```text
O(N)
```

例如：

```text
1 2 3 4 5

插入到2后面

1 2 x 3 4 5
```

后面的元素都要向后移动。

---

#### list

链表插入：

```cpp
lst.insert(pos, x);
```

只需要修改几个指针：

```text
prev -> newNode -> next
```

时间复杂度：

```text
O(1)
```

删除同理：

```cpp
lst.erase(pos);
```

也是 O(1)。

因此频繁中间插入删除时，list 更有优势。

---

4. 内存占用

#### vector

每个元素只保存数据：

```text
[data]
```

额外开销很小。

---

#### list

每个节点除了数据外还需要：

```text
prev指针
next指针
```

例如 64 位系统：

```text
8B prev
8B next
```

因此链表内存开销明显更大。

---

5. 缓存友好性

这是实际项目中非常重要的一点。

#### vector

连续内存：

```text
1 2 3 4 5 6 7 8
```

CPU 预取效果好。

访问时：

```cpp
for(auto& x : v)
```

通常缓存命中率较高。

---

#### list

节点分散：

```text
node1 -> 0x1000
node2 -> 0x8000
node3 -> 0x3000
```

CPU 很难预取。

缓存命中率较低。

因此即使理论上 list 的插入删除复杂度更优，很多实际场景下 vector 依然更快。

---

6. 迭代器失效

#### vector

扩容时：

```cpp
push_back()
```

可能导致整块内存重新申请。

此时：

```cpp
iterator
pointer
reference
```

全部失效。

---

#### list

节点独立存在。

插入删除某个节点时：

```text
其它节点迭代器仍然有效
```

只有被删除节点对应的迭代器失效。

---

**总结**

| 对比项   | vector  | list  |
| ----- | ------- | ----- |
| 底层结构  | 动态数组    | 双向链表  |
| 内存布局  | 连续      | 非连续   |
| 随机访问  | O(1)    | O(N)  |
| 尾部插入  | O(1)    | O(1)  |
| 中间插入  | O(N)    | O(1)  |
| 中间删除  | O(N)    | O(1)  |
| 缓存友好  | 好       | 差     |
| 内存开销  | 小       | 大     |
| 迭代器类型 | 随机访问迭代器 | 双向迭代器 |

因此在实际开发中，如果主要需求是存储和访问数据，我通常优先选择 vector，因为它缓存友好、访问速度快；只有在需要频繁进行中间位置插入和删除，并且已经持有对应迭代器的场景下，我才会考虑使用 list。


### 堆上初始化和栈上初始化
| 特性 | 栈上初始化 | 堆上初始化 |
| :-- | :-- | :-- | :-- |
| 内存区域 | 栈Stack | 堆Heap |
| 分配速度 | 快(移动指针) | 慢(需要查找空闲内存) |
| 管理方式 | 自动管理，作用域结束自动释放 | 手动管理，需要`delete` |
| 生命周期 | 随作用域 | 手动控制 |
| 大小限制 | 较小，通常为1-8MB | 较大 |
| 分配时机 | 编译时确定大小 | 运行时动态分配 |

栈上初始化 
```cpp
// 1. 基础类型
int a = 10;
int b(20);      // 直接初始化
int c{30};      // C++11 统一初始化

// 2. 数组
int arr[10];                    // 固定大小
int arr2[5] = {1, 2, 3, 4, 5};

// 3. 对象
std::string str = "hello";
MyClass obj;
MyClass obj2(10, "test");
MyClass obj3{10, "test"};       // C++11
```
```text
// 内存布局
栈内存（高地址 → 低地址）
┌─────────────────────────────┐  ← 栈顶
│  局部变量 str                │
├─────────────────────────────┤
│  局部变量 obj                │
├─────────────────────────────┤
│  局部变量 arr[10]           │
├─────────────────────────────┤
│  main 函数返回地址           │
├─────────────────────────────┤
│  ...                        │
└─────────────────────────────┘  ← 栈底
```

堆上初始化
```cpp
// C 风格（不推荐在 C++ 中使用）
int* p = (int*)malloc(sizeof(int));
*p = 10;
free(p);

// C++ 传统方式
int* p1 = new int(10);           // 单个变量
int* p2 = new int[10];           // 数组
MyClass* obj = new MyClass();    // 对象
MyClass* obj2 = new MyClass(10, "test");

// 必须手动释放
delete p1;
delete[] p2;
delete obj;
delete obj2;

// C++14/17 推荐方式（智能指针）
std::unique_ptr<int> up = std::make_unique<int>(10);
std::unique_ptr<MyClass> uobj = std::make_unique<MyClass>(10, "test");

std::shared_ptr<MyClass> sp = std::make_shared<MyClass>(10, "test");

// 自动释放，无需手动 delete
```
```text
// 内存布局
堆内存（低地址 → 高地址）
┌─────────────────────────────┐
│  空闲内存块                  │
├─────────────────────────────┤
│  new MyClass 分配的内存      │ ← obj 指向这里
├─────────────────────────────┤
│  new int[10] 分配的内存      │ ← p2 指向这里
├─────────────────────────────┤
│  空闲内存块                  │
├─────────────────────────────┤
│  ...                        │
└─────────────────────────────┘

栈上（持有堆内存的指针）
┌─────────────────────────────┐
│  MyClass* obj ──────────────┼──→ 指向堆上的对象
├─────────────────────────────┤
│  int* p2 ───────────────────┼──→ 指向堆上的数组
└─────────────────────────────┘
```

### 手撕vector的resize

resize和reserve
| 操作 | `resize` | `reserve` |
| :-- | :-- | :-- |
| 改变size | 是 | 否 |
| 改变capacity | 可能(when n > capacity) | 是(增加到>=n) |
| 触发重新分配 | 仅当`n > capacity` | 同`resize` |

```cpp
#include <iostream>
#include <vector>

int main() {
    std::vector<int> vec;
    
    // 情况1：当前 size = 0, capacity = 0
    vec.resize(5);   // 5 > capacity(0) → 重新分配，构造 5 个零值元素
    std::cout << "size=" << vec.size() << ", cap=" << vec.capacity() << "\n";
    // 输出：size=5, cap=5（或更大）
    
    // 情况2：当前 size = 5, capacity = 5
    vec.resize(3);   // 3 < size → 不重新分配，销毁末尾 2 个元素
    std::cout << "size=" << vec.size() << ", cap=" << vec.capacity() << "\n";
    // 输出：size=3, cap=5（容量不变）
    
    // 情况3：当前 size = 3, capacity = 5
    vec.resize(8);   // 8 > capacity → 重新分配，容量增长到至少 8
    std::cout << "size=" << vec.size() << ", cap=" << vec.capacity() << "\n";
    // 输出：size=8, cap=10（通常翻倍策略）
}
```

### STL中map的底层是如何实现的？红黑树为什么是相对平衡的？为什么不选择AVL？
1. map底层使用红黑树实现，他满足：
   1. 元素按照Key自动有序
   2. 查找、插入、删除效率稳定
2. 红黑树是什么
本质：二叉搜索树
在BST基础上增加颜色属性
- 每个节点非黑即红
- 根节点必须为黑色
- 所有叶节点(**NULL空节点，即空指针对应的哨兵节点**)都是黑色
- 红色节点不能连续出现
- 从任意节点到其叶子节点路径上的黑色节点数量相同
3. 为什么不选AVL而是红黑树？
AVL（平衡二叉树）为一颗空树或者左右两个字数高度差绝对值不超过1，它的查找速度更快
但是，AVL树插入、删除调整更多，旋转次数更多

红黑树查找稍慢，但是插入删除快、旋转少，面对map的经典场景（频繁插入、删除、查找）具有更好的综合性能。

### 以vector的新方法为例，介绍你了解的C++11新特性
1. `push_back`和移动语义
C++11之前：
```cpp
vector<string> vec; 
string str = "hello"; 
vec.push_back(str);
```
调用拷贝构造`string(const string&);`，产生一次对象拷贝

C++11引入右值引用后：：
```cpp
vec.push_back(std::move(str));
```
调用移动构造`string(const string&&);`转移指针所有权，直接管理内部资源

2. `emplace_back`
传统`push_back`先构造临时对象，再将其拷贝/移动到vector，至少涉及一次额外对象构造
`emplace_back`直接在容器内部构造，效率更高

3. 扩容优化
4. 统一初始化
5. 范围for循环

### 能否自己写一个RAII类？
```cpp
#include <iostream>
#include <stdexcept>
#include <cstring>

// 自定义RAII类，管理动态内存
template<typename T>
class My_unique_ptr{
private:
    T* ptr;     // 对象

public:
    // 构造函数，获取资源
    explicit My_unique_ptr(T* p = nullptr): ptr(p) {
        std::cout << "获取资源：" << ptr << std::endl;
    }
    // 析构函数，释放资源
    ~ My_unique_ptr() {
        if (ptr) {
            std::cout << "释放资源：" << ptr <<std::endl;
        }
        delete ptr;
    }
    // 禁止拷贝，防止double delete
    My_unique_ptr(const My_unique_ptr&) = delete;
    My_unique_ptr& opetator=(const My_unqiue_ptr&) = delete;
    // 移动语义，转移资源
    My_unique_ptr(My_unique_ptr&& other) noexcept : ptr(other.ptr) {
        other.ptr = nullptr;
        std::cout << "移动构造，转移所有权" << std::endl;
    }
    My_unique_ptr& operator=(My_unique_ptr&& other) noexcept {
        if (this != &other) {
            delete ptr; // 释放当前资源
            ptr = other.ptr;    // 接管新资源
            other.ptr = nullptr;
            std::cout << "移动幅值，转移所有权" << endl;
        }
        return *this;
    }
    // 5. 访问接口
    T& operator*() const { return *ptr; }
    T* operator->() const { return ptr; }
    T* get() const { return ptr; }

    // 6. 显式释放（可选）
    void reset(T* p = nullptr) {
        if (ptr != p) {
            delete ptr;
            ptr = p;
        }
    }
};

// 使用示例
class Person {
public:
    int age;

    Person(const std::string& n, int a) : name(n), age(a) {
        std::cout << "Person构造" << name << std::endl;
    }
    ~Person() {
        std::cout << "Person析构" << name << std::endl;
    }
    void speak() const {
        std::cout << "我是 " << name << "，今年 " << age << " 岁" << std::endl;
    }
};

int main() {
    std::cout << "=== 测试1：正常生命周期 ===\n";
    {
        My_unique_ptr<Person> p1(new Person("Alice", 25));
        p1->speak();  // 使用箭头运算符
        (*p1).speak(); // 使用解引用运算符
        // p1 离开作用域时自动释放
    }
    std::cout << "p1 已析构，内存已释放\n\n";

    std::cout << "=== 测试2：移动语义 ===\n";
    {
        My_unique_ptr<Person> p2(new Person("Bob", 30));
        My_unique_ptr<Person> p3 = std::move(p2);  // 移动构造，所有权转移
        
        if (p2.get() == nullptr) {
            std::cout << "p2 现在是空指针\n";
        }
        p3->speak();
        // p3 离开作用域时释放资源
    }
    std::cout << "p3 已析构，内存已释放\n\n";

    std::cout << "=== 测试3：异常安全 ===\n";
    try {
        My_unique_ptr<Person> p4(new Person("Charlie", 35));
        throw std::runtime_error("发生异常！");
        // 即使抛异常，p4 的析构函数依然会被调用
    } catch (const std::exception& e) {
        std::cout << "捕获异常: " << e.what() << std::endl;
        // p4 已经在栈展开时被析构，内存已释放
    }

    return 0;
}
```

### 介绍一下进程和线程的区别
进程（Process）：

- 操作系统进行资源分配的基本单位
- 程序的一次执行过程，拥有独立的内存空间
- 每个进程都有自己的代码段、数据段、堆栈段

线程（Thread）：

- 操作系统进行CPU调度的基本单位
- 进程内的一个执行流，共享进程的资源
- 同一个进程内的多个线程共享代码段、数据段、堆段，但有自己的栈段

```text
┌─────────────────────────────────────┐
│              进程                   │
│  ┌─────────────────────────────────┐│
│  │   代码段、数据段、堆段           │ │
│  │   (所有线程共享)                 ││
│  └─────────────────────────────────┘│
│  ┌─────┐  ┌─────┐  ┌─────┐          │
│  │线程1 │ │线程2 │ │线程3 │          │
│  │ 栈  │  │ 栈  │  │ 栈  │          │
│  └─────┘  └─────┘  └─────┘          │
└─────────────────────────────────────┘
```

### 介绍一下TCP三次握手
TCP三次握手建立连接:
```txt
客户端（主动）                  服务器（被动）
   |                              |
   | ------ SYN (SEQ=x) --------> |  (1) 请求连接
   |                              |
   | <--- SYN+ACK (SEQ=y, ACK=x+1) |  (2) 确认并请求
   |                              |
   | --- ACK (SEQ=x+1, ACK=y+1) -> |  (3) 最终确认
   |                              |
   | ==== 连接建立，开始传输数据 ==== |
```

- **为什么不是两次握手**：如果只有两次，服务器发出确认后即认为连接建立，如果客户端没有收到，则会造成服务器资源浪费。三次握手确保双方都有收发能力
- **什么是SYN洪水攻击**：恶意客户端只发送第一次握手，不回应第三次，导致服务器挂起大量半连接，浪费资源

TCP四次握手断开连接：
```txt
主动关闭方 (Client)               被动关闭方 (Server)
   |                                    |
   | ---- FIN (SEQ=u) ----------------> | (1) 主动方说“我不发了”
   |                                    |
   | <--- ACK (SEQ=v, ACK=u+1) -------- | (2) 被动方确认，但可能还有数据要发
   |                                    |
   |      (此时被动方继续发送剩余数据)      |
   |                                    |
   | <--- FIN (SEQ=w) ---------------- | (3) 被动方说“我也发完了”
   |                                    |
   | ---- ACK (SEQ=w+1) --------------> | (4) 主动方最后确认
   |                                    |
   | (等待 2MSL 后关闭)                  | (收到ACK后立即关闭)
```

- **为什么不是三次握手**：第二次和第三次握手不能合并，被动放需要把剩余数据发完
- **TIME_WAIT状态**：主动关闭方发送随后一次ACK后，等待**2倍最大报文时间**
  - 确保被动房收到最后的ACK。如果没收到，被动方会重发FIN，主动方还能重发ACK
  - 让网络中残留的旧数据包全部消失，避免影响下一次使用相同端口的新连接

### 网络封装格式
| 层级 | 封装格式 |
| :-- | :-- |
| 应用层 | 应用数据 |
| 传输层 | TCP头-应用数据 |
| 网络层 | IP头-TCP头-应用数据 |
| 链路层 | 帧头(MAC头)-IP头-TCP头-应用数据-帧尾(FCS校验) |
| 物理层 | (进行比特流传输) |

### 介绍一下键入网址到网页显示的流程
- 浏览器解析URL，确认Web服务器和文件名，生成`HTTP`请求
- DNS查询真实地址
  - 浏览器解析URL、生成HTTP消息后，需要委托操作系统将消息发送给Web服务器
  - DNS服务器存储了Web服务器域名与IP的对应关系
- 通过DNS获取IP后，将HTTP传输工作交给操作系统的协议栈
- 进行TCP可靠传输，为数据包添加TCP头
- IP模块将数据封装为网络包发送给通信对象，为其添加IP头
- 在IP头前加上MAC头，包含收发双方的MAC地址等信息
  - 发送方MAC地址在网卡生产时写入ROM中
  - 接收方MAC地址通过ARP(地址解析)协议，在以太网中广播询问IP地址对应设备的MAC地址
- 网卡添加起始帧和校验帧，将保转为电信号通过网线发送
- 交换机转发网络包
  - 交换机根据MAC地址表查找MAC地址，将信号发送到相应端口
  - 如果没查找到则进行广播
- 路由器转发数据包
  - 接收包，将电信好在换为数字信号
  - 查询路由表确定输出端口
  - 发送数据包
- 接收方接受并解析数据包