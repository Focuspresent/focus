#ifndef __FOCUS_FIBER_H__
#define __FOCUS_FIBER_H__

#include <functional>
#include <memory>
#include <ucontext.h>
#include <cstdint>

namespace focus {

/**
 * @brief 协程类
 */
class Fiber: public std::enable_shared_from_this<Fiber> {
public:
    typedef std::shared_ptr<Fiber> ptr;

    /**
     * 协程状态
     */
    enum State {
        // 就绪态,刚创建或者yield后的状态
        READY = 0,
        // 运行态,resume后的状态 
        RUNNING, 
        // 结束态,协程的回调函数结束后的状态
        TERM 
    };

private:
    /**
     * @brief 无参构造
     * @attention 只能创建线程的第一个协程，只能由GetThis调用
     */
    Fiber();

public:
    /**
     * @brief 构造函数
     * @param[in] cb 协程入口函数
     * @param[in] stacksize 协程栈大小
     * @param[in] runInScheduler 是否参与调度器
     */
    Fiber(std::function<void()> cb, uint32_t stacksize = 0, bool runInScheduler = true);

    /**
     * @brief 析构函数
     */
    ~Fiber();

    /**
     * @brief 重置入口函数和协程状态，复用栈空间
     * @param[in] cb 协程入口函数
     */
    void reset(std::function<void()> cb);

    /**
     * @brief 将当前协程切换到执行状态
     * @details 交换当前协程和正在运行的协程，前者变成RUNNING，后者变成READY
     */
    void resume();

    /**
     * @brief 当前协程让出执行权
     * @details 交换当前协程和上一次resume的协程，前者变成READY，后缀变成RUNNING
     */
    void yield();

    /**
     * @brief 获取协程id
     */
    uint64_t getId() const {
        return m_id;
    }

    /**
     * @brief 获取协程状态
     */
    State getState() const {
        return m_state;
    }

public:
    /**
     * @brief 设置当前协程
     */
    static void SetThis(Fiber* f);

    /**
     * @brief 获取当前执行的协程
     */
    static Fiber::ptr GetThis();

    /**
     * @brief 获取协程总数
     */
    static uint64_t TotalFibers();

    /**
     * @brief 协程入口函数
     */
    static void MainFunc();

    /**
     * @brief 获取当前协程id
     */
    static uint64_t GetFiberId();

private:
    uint64_t m_id = 0; // 协程id
    uint32_t m_stacksize = 0; // 协程栈大小
    State m_state = READY; // 协程状态
    ucontext_t m_uctx; // 协程上下文
    void* m_stack = nullptr; // 协程栈地址
    std::function<void()> m_cb; // 协程回调函数
    bool m_runInScheduler; // 是否参与调度器
};

} // namespace focus

#endif