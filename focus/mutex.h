#ifndef __FOCUS_MUTEX_H__
#define __FOCUS_MUTEX_H__

#include <atomic>
#include <semaphore.h>
#include <pthread.h>
#include <stdint.h>

#include "nocopyable.h"

namespace focus{

/**
 * @brief 信号量
 * @attention 会抛出异常
 */
class Semaphore:public Nocopyable{
public:
    // 构造
    Semaphore(uint32_t count=0);

    // 析构
    ~Semaphore();

    // 获取信号量
    void wait();

    // 释放信号量
    void notify();
private:
    sem_t semaphore_;
};

// 局部锁模版
template<class T>
class ScopedLockImpl{
public:
    using value_ref=T&;

    ScopedLockImpl(value_ref mutex):
        mutex_(mutex){  
        mutex_.lock();
        locked_=true;
    }

    ~ScopedLockImpl(){
        unlock();
    }   

    void lock(){
        if(!locked_){
            mutex_.lock();
            locked_=true;
        }
    }

    void unlock(){
        if(locked_){
            mutex_.unlock();
            locked_=false;
        }
    }

private:
    value_ref mutex_;
    std::atomic<bool> locked_=false;
};

// 局部读锁模版
template<class T>
class ReadScopedLockImpl{
public:
    using value_ref=T&;

    ReadScopedLockImpl(value_ref mutex):
        mutex_(mutex){  
        mutex_.rdlock();
        locked_=true;
    }

    ~ReadScopedLockImpl(){
        unlock();
    }   

    void lock(){
        if(!locked_){
            mutex_.rdlock();
            locked_=true;
        }
    }

    void unlock(){
        if(locked_){
            mutex_.unlock();
            locked_=false;
        }
    }

private:
    value_ref mutex_;
    std::atomic<bool> locked_=false;
};

// 局部写锁模版
template<class T>
class WriteScopedLockImpl{
public:
    using value_ref=T&;

    WriteScopedLockImpl(value_ref mutex):
        mutex_(mutex){  
        mutex_.wrlock();
        locked_=true;
    }

    ~WriteScopedLockImpl(){
        unlock();
    }   

    void lock(){
        if(!locked_){
            mutex_.wrlock();
            locked_=true;
        }
    }

    void unlock(){
        if(locked_){
            mutex_.unlock();
            locked_=false;
        }
    }

private:
    value_ref mutex_;
    std::atomic<bool> locked_=false;
};

// 互斥量
class Mutex: public Nocopyable{
public:
    // 互斥量别名
    using Lock=ScopedLockImpl<Mutex>;

    // 构造
    Mutex(){
        pthread_mutex_init(&mutex_,nullptr);
    }

    // 析构
    ~Mutex(){
        pthread_mutex_destroy(&mutex_);
    }

    // 上锁
    void lock(){
        pthread_mutex_lock(&mutex_);
    }

    // 解锁
    void unlock(){
        pthread_mutex_unlock(&mutex_);
    }

private:
    pthread_mutex_t mutex_;
};


// 空锁(用于调试)
class NullMutex: public Nocopyable{
public:
    // 别名
    using Lock=ScopedLockImpl<Mutex>;

    // 构造
    Mutex() {}

    // 析构
    ~Mutex() {}

    // 上锁
    void lock() {}

    // 解锁
    void unlock() {}
};

// 读写互斥量
class RWMutex: public Nocopyable{
public:
    // 读锁别名
    using ReadLock=ReadScopedLockImpl<RWMutex>;
    // 写锁别名
    using WriteLock=WriteScopedLockImpl<RWMutex>;

    // 构造
    RWMutex(){
        pthread_rwlock_init(&lock_,nullptr);
    }

    // 析构
    ~RWMutex(){
        pthread_rwlock_destroy(&lock_);
    }

    // 上读锁
    void rdlock(){
        pthread_rwlock_rdlock(&lock_);
    }

    // 上写锁
    void wrlock(){
        pthread_rwlock_wrlock(&lock_);
    }

    // 释放锁
    void unlock(){
        pthread_rwlock_destroy(&lock_);
    }

private:
    pthread_rwlock_t lock_;
};

// 空读写锁
class NullRWMutex: public Nocopyable{
public:
    // 读锁别名
    using ReadLock=ReadScopedLockImpl<RWMutex>;
    // 写锁别名
    using WriteLock=WriteScopedLockImpl<RWMutex>;

    // 构造
    RWMutex() {}

    // 析构
    ~RWMutex() {}

    // 上读锁
    void rdlock() {}

    // 上写锁
    void wrlock() {}

    // 释放锁
    void unlock() {}
};

// 自旋锁
class SpinLock: public Nocopyable{
public:
    // 别名
    using Lock=ScopedLockImpl<SpinLock>;

    // 构造
    SpinLock(){
        pthread_spin_init(&mutex_);
    }

    // 析构
    ~SpinLock(){
        pthread_spin_destroy(&mutex_);
    }

    // 上锁
    void lock(){
        pthread_spin_lock(&mutex_);
    }

    // 解锁
    void unlock(){
        pthread_spin_unlock(&mutex_);
    }

priavte:
    pthread_spinlock_t mutex_;
};

// 原子锁
class CASLock: public Nocopyable{
public:
    // 别名
    using Lock=ScopedLockImpl<CASLock>;

    // 构造
    CASLock(){
        mutex_.clear();
    }

    // 析构
    ~CASLock() {}

    // 上锁
    void lock(){
        // 确保写操作能被后续读操作读取
        while(std::atomic_flag_test_and_set_explicit(&mutex_,std::memory_order_acquire));
    }

    // 解锁
    void unlock(){
        // 保证写操作在之前完成
        std::atomic_flag_clear_explicit(&mutex_,std::memory_order_release);
    }

private:
    volatile std::atomic_flag mutex_;
};

}

#endif