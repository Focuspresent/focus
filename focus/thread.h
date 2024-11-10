#ifndef __FOCUS_THREAD_H__
#define __FOCUS_THREAD_H__

#include <functional>
#include <string>
#include <memory>

#include "nocopyable.h"
#include "mutex.h"
#include "util.h"

namespace focus {

class Thread: public Nocopyable {
public:
    // 别名
    using ptr=std::shared_ptr<Thread>;

    // 构造
    Thread(std::function<void()> cb,const std::string& name);

    // 析构
    ~Thread();

    // 获取线程id
    pid_t getId() const { return m_id; }

    // 获取线程名
    const std::string& getName() const { return m_name; }

    // 等待线程执行完成
    void join();

    // 获取当前线程的指针
    static Thread* GetThis();

    // 获取当前线程的名字
    static const std::string& GetName();

    // 设置当前线程的名字
    static void SetName(const std::string& name);
private:
    // 线程执行的函数
    static void* run(void* arg);

private:
    pid_t m_id=-1; // 线程id
    pthread_t m_thread=0; // 线程结构
    std::function<void()> m_cb; // 线程执行的函数
    std::string m_name; // 线程名
    Semaphore m_semaphore; // 信号量
};

}

#endif