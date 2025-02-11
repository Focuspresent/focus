#include "fiber.h"
#include "log.h"
#include "macro.h"
#include "config.h"
#include "scheduler.h"
#include <atomic>

namespace focus {

// 全局的日志器
static Logger::ptr g_logger = FOCUS_LOG_NAME("system");

// 全局静态变量，协程id
static std::atomic<uint64_t> s_fiber_id = {0};
// 全局静态变量，协程总数
static std::atomic<uint64_t> s_fiber_count = {0};

// 线程局部变量，当前运行的协程原始指针
static thread_local Fiber* t_fiber = nullptr;
// 线程局部变量，当前线程的主线程
static thread_local Fiber::ptr t_thread_fiber = nullptr;

// 协程栈大小
static ConfigVar<uint32_t>::ptr g_fiber_stack_size = 
    Config::LookUp<uint32_t>("fiber.stack_size", 128 * 1024, "fiber stack size");

/**
 * @brief 栈内存分配器
 */
class MallocStackAllocator {
public:
    static void* Alloc(size_t size) {
        return malloc(size);
    }

    static void Dealloc(void* vp, size_t size) {
        return free(vp);
    }
};

using StackAllocator = MallocStackAllocator;

Fiber::Fiber() {
    // 设置当前协程指针    
    SetThis(this);
    // 置为运行状态
    m_state = RUNNING;

    // 获取上下文
    if(getcontext(&m_uctx)) {
        FOCUS_ASSERT2(false, "Fiber::Fiber() getcontext");
    }

    // 总数加1
    ++s_fiber_count;
    m_id = s_fiber_id++;

    FOCUS_LOG_DEBUG(g_logger) << "Fiber::Fiber() main id = " << m_id;
}

/**
 * @brief 创建子协程，需要分配栈
 */
Fiber::Fiber(std::function<void()> cb, uint32_t stacksize, bool runInScheduler):
    m_id(s_fiber_id++),
    m_cb(cb),
    m_runInScheduler(runInScheduler) {
    ++s_fiber_count;
    m_stacksize = stacksize? stacksize: g_fiber_stack_size->getVal();
    m_stack = StackAllocator::Alloc(m_stacksize);

    // 获取上下文
    if(getcontext(&m_uctx)) {
        FOCUS_ASSERT2(false, "Fiber:Fiber(...) getcontext");
    }

    // 填充上下文信息
    m_uctx.uc_link = nullptr;
    m_uctx.uc_stack.ss_sp = m_stack;
    m_uctx.uc_stack.ss_size = m_stacksize;

    // 绑定入口函数
    makecontext(&m_uctx, &Fiber::MainFunc, 0);

    // 调试信息
    FOCUS_LOG_DEBUG(g_logger) << "Fiber::Fiber(...) id = " << m_id;
}

/**
 * @brief 根据不同协程析构
 */
Fiber::~Fiber() {
    FOCUS_LOG_DEBUG(g_logger) << "Fiber::~Fiber() id = " << m_id;
    --s_fiber_count;
    if(m_stack) {
        // 子协程
        // 确保是结束状态
        FOCUS_ASSERT(TERM == m_state);
        // 释放内存
        StackAllocator::Dealloc(m_stack, m_stacksize);
        // 调试信息
        FOCUS_LOG_DEBUG(g_logger) << "dealloc stack, id = " << m_id;
    }else {
        // 主协程
        // 没有回调
        FOCUS_ASSERT(!m_cb);
        // 运行状态
        FOCUS_ASSERT(RUNNING == m_state);

        // 当前协程是自己
        Fiber* cur = t_fiber;
        if(this == cur) {
            SetThis(nullptr);
        }
    }
}

/**
 * @brief 只有结束的子协程可以重置
 */
void Fiber::reset(std::function<void()> cb) {
    // 确保是结束的子协程
    FOCUS_ASSERT(m_stack);
    FOCUS_ASSERT(TERM == m_state);

    // 重载信息
    m_cb = cb;

    // 获取上下文
    if(getcontext(&m_uctx)) {
        FOCUS_ASSERT2(false, "Fiber::reset getcontext");
    }

    // 填充上下文信息
    m_uctx.uc_link = nullptr;
    m_uctx.uc_stack.ss_sp = m_stack;
    m_uctx.uc_stack.ss_size = m_stacksize;

    // 绑定入口函数
    makecontext(&m_uctx, &Fiber::MainFunc, 0);
    m_state = READY;
}

/**
 * @brief 与主协程交换，进入运行状态
 */
void Fiber::resume() {
    FOCUS_ASSERT(TERM != m_state && RUNNING != m_state);
    SetThis(this);

    m_state = RUNNING;

    // 是否参加调度器
    if(m_runInScheduler) {
        // 与调度器的主协程交换
        if(swapcontext(&(Scheduler::GetMainFiber()->m_uctx), &m_uctx)) {
            FOCUS_ASSERT2(false, "Fiber::resume() swapcontext from thread to scheduler");
        }
    }else {
        // 与当前线程的主协程交换
        if(swapcontext(&(t_thread_fiber->m_uctx), &m_uctx)) {
            FOCUS_ASSERT2(false, "Fiber::resume() swapcontext from thread to cur");
        }
    }
}

/**
 * @brief 与主协程交换，进入等待状态
 * @attention 结束的子协程，会自动让出执行权
 */
void Fiber::yield() {
    FOCUS_ASSERT(TERM == m_state || RUNNING == m_state);
    SetThis(t_thread_fiber.get());

    if(TERM != m_state) {
        m_state = READY;
    }

    // 是否参加调度器
    if(m_runInScheduler) {
        // 与调度器的主协程交换
        if(swapcontext(&m_uctx, &(Scheduler::GetMainFiber()->m_uctx))) {
            FOCUS_ASSERT2(false, "Fiber::yield() swapcontext from scheduler to thread");
        }
    }else {
        // 与当前线程的主协程交换
        if(swapcontext(&m_uctx, &(t_thread_fiber->m_uctx))) {
            FOCUS_ASSERT2(false, "Fiber::yield() swapcontext from cur to thread");
        }
    }
}

/**
 * @brief 设置当前协程
 */
void Fiber::SetThis(Fiber* f) {
    t_fiber = f;
}

/**
 * @brief 获取当前协程，同时初始化协程主协程，使用前需要调用
 */
Fiber::ptr Fiber::GetThis() {
    if(t_fiber) {
        return t_fiber->shared_from_this();
    }

    // 创建线程主协程
    Fiber::ptr main_fiber(new Fiber());
    // 断言空构造的设置当前协程是否成功
    FOCUS_ASSERT(main_fiber.get() == t_fiber);
    // 保存当前线程主协程
    t_thread_fiber = main_fiber;
    return t_fiber->shared_from_this();
}
 
uint64_t Fiber::TotalFibers() {
    return s_fiber_count.load();
}

/**
 * @brief 协程入口函数
 */
void Fiber::MainFunc() {
    Fiber::ptr cur = GetThis(); // 引用计数加1
    FOCUS_ASSERT(cur);

    // 调用函数，并进入结束状态
    cur->m_cb();
    cur->m_cb = nullptr;
    cur->m_state = TERM;

    auto raw_ptr = cur.get(); // 引用计数减1
    cur.reset();
    // 让出执行权
    raw_ptr->yield();
}

uint64_t Fiber::GetFiberId() {
    if(t_fiber) {
        return t_fiber->getId();
    }

    return 0;
}

} // namespace focus