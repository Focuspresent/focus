#ifndef __FOCUS_SINGLETON_H
#define __FOCUS_SINGLETON_H

#include <memory>

// 单例模式封装类
template<class T,class X=void,int N=0>
class Singleton{
public:
    // 返回单例裸指针
    static T* GetInstance(){
        static T v;
        return &v;
    }
};

// 单例模式智能指针
template<class T,class X=void,int N=0>
class SingletonPtr{
public:
    // 返回单例智能指针
    static std::share_ptr<T> GetInstance(){
        std::share_ptr<T> v(new T);
        return v;
    }
};

#endif