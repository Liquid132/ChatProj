#pragma once
/*
单例类。对于任意类型的T，在整个程序运行期间，有且只有一个T的实例存在，
    并且会提供一个全局访问点来获取该实例。同时要考虑多线程环境下的初
    始化安全
*/
#include <memory>
#include <mutex>
#include <iostream>
template <typename T>
class Singleton {
protected:
    //外部代码无法直接实例化Singleton<T>但是类T可以继承自Singleton<T>
    //（即奇异递归模板模式）

    //显示要求编译器为SIngleton类生成一个默认的无参数构造函数
    Singleton() = default;
    //显示删除拷贝和赋值运算符
    Singleton(const Singleton<T>&) = delete;
    Singleton& operator=(const Singleton<T>& st) = delete;
    //静态智能指针用于保留实例
    static std::shared_ptr<T> _instance;
public:
    static std::shared_ptr<T> GetInstance() {
        static std::once_flag s_flag;
        std::call_once(s_flag, [&]() {
            _instance = std::shared_ptr<T>(new T);
            });

        return _instance;
    }
    void PrintAddress() {
        std::cout << _instance.get() << std::endl;
    }
    ~Singleton() {
        std::cout << "this is singleton destruct" << std::endl;
    }
};

template <typename T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;