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
new和new[]会在内存中记录不同元数据，后者会在内存块头部记录数组大小（元素个数），delete[]会读取这个信息柱哥调用析构函数；delete只会调用第一个元素的析构函数

### 手撕memcpy
