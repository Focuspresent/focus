#include "scheduler.h"
#include "macro.h"

namespace focus {

// 全局的日志器
static Logger::ptr g_logger = FOCUS_LOG_NAME("system");

// 当前线程的调度器实例
static thread_local Scheduler* t_scheduler = nullptr;
// 当前线程的调度协程
static thread_local Fiber* t_scheduler_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool useCaller, const std::string name) {
    // 判断线程数
    FOCUS_ASSERT(threads > 0);

    m_useCaller = useCaller;
    m_name = name;

    // 如果要当前线程要参与
    if(useCaller) {
        --threads;
        Fiber::GetThis();
        FOCUS_ASSERT(GetThis() == nullptr);
        t_scheduler = this;

        /**
         * caller的主协程不参与调度，在调度协程结束后，应返回caller线程
         * userCaller为true时，要保存主协程，等调度协程结束
         */
        m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, false));

        // 设置值
        Thread::SetName(m_name);
        t_scheduler_fiber = m_rootFiber.get();
        m_rootThread = GetThreadId();
        m_threadIds.emplace_back(m_rootThread);
    }else {
        m_rootThread = -1;
    }
    m_threadCount = threads;
}

Scheduler::~Scheduler() {
    FOCUS_LOG_DEBUG(g_logger) << "Scheduler::~Scheduler()";
    FOCUS_ASSERT(m_stopping);
    if(GetThis() == this) {
        t_scheduler = nullptr;
    }
}

Scheduler* Scheduler::GetThis() {
    return t_scheduler;
}

Fiber* Scheduler::GetMainFiber() {
    return t_scheduler_fiber;
}

void Scheduler::start() {
    FOCUS_LOG_DEBUG(g_logger) << "start";
    MutexType::Lock lock(m_mutex);
    if(m_stopping) {
        FOCUS_LOG_ERROR(g_logger) << "Scheduler is stopped";
        return ;
    }
    FOCUS_ASSERT(m_threads.empty());
    m_threads.resize(m_threadCount);
    for(size_t i = 0; i < m_threadCount; ++i) {
        m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
        m_threadIds.emplace_back(m_threads[i]->getId());
    }
}

void Scheduler::stop() {
    FOCUS_LOG_DEBUG(g_logger) << "stop";
    /*if(isCanStop()) {
        return ;
    }*/
    m_stopping = true;

    // 如果是usecaller
    if(m_useCaller) {
        FOCUS_ASSERT(GetThis() == this);
    }else {
        FOCUS_ASSERT(GetThis() != this);
    }

    for(size_t i = 0; i < m_threadCount; ++i) {
        tickle();
    }

    if(m_rootFiber) {
        tickle();
    }

    // 在usecaller
    if(m_rootFiber) {
        m_rootFiber->resume();
        FOCUS_LOG_DEBUG(g_logger) << "rootFiber end";
    }

    // 换出所有线程
    std::vector<Thread::ptr> threads;
    {
        MutexType::Lock lock(m_mutex);
        threads.swap(m_threads);
    }

    // 等待线程结束
    for(auto& thr: threads) {
        thr->join();
    }
}

void Scheduler::tickle() {
    FOCUS_LOG_DEBUG(g_logger) << "tickle";
}

void Scheduler::run() {
    FOCUS_LOG_DEBUG(g_logger) << "run";
    // TODO hook
    setThis();
    // 其余非caller调度
    if(GetThreadId() != m_rootThread) {
        t_scheduler_fiber = Fiber::GetThis().get();
    }

    // 存储空闲协程，回调协程
    Fiber::ptr idleFiber(new Fiber(std::bind(&Scheduler::idle, this)));
    Fiber::ptr cbFiber;

    // 存储拿到的调度任务
    ScheduleTask task;
    while(true) {
        task.reset();
        bool tickleMe = false; // 是否tickle其他线程进行调度
        {
            MutexType::Lock lock(m_mutex);
            auto it = m_tasks.begin();
            // 遍历所有调度任务
            while(it != m_tasks.end()) {
                if(it->m_thread != -1 && it->m_thread != GetThreadId()) {
                    // 指定了线程号，但不是当前线程
                    ++it;
                    tickleMe = true;
                    continue;
                }

                // 没有实际执行对象
                FOCUS_ASSERT(it->m_fiber || it->m_cb);

                // 多线程高并发情境下，有可能发生刚添加事件就被触发的情况，如果此时当前协程还未来得及yield，则这里就有可能出现协程状态仍为RUNNING的情况
                if(it->m_fiber && Fiber::RUNNING == it->m_fiber->getState()) {
                    ++it;
                    continue;
                }

                // 拿到调度任务
                task = *it;
                m_tasks.erase(it++);
                ++m_activeThreadCount;
                break;
            }
            // 当前线程拿完还有任务
            tickleMe |= (m_tasks.end() != it);
        }

        // 需要通知
        if(tickleMe) {
            tickle();
        }

        // 执行
        if(task.m_fiber) {
            // resume协程
            task.m_fiber->resume();
            --m_activeThreadCount;
            task.reset();
        }else if(task.m_cb) {
            // 将函数包装成协程
            if(cbFiber) {
                cbFiber->reset(task.m_cb);
            }else {
                cbFiber.reset(new Fiber(task.m_cb));
            }
            task.reset();
            cbFiber->resume();
            --m_activeThreadCount;
            cbFiber.reset();
        }else {
            // 任务队列空
            if(Fiber::TERM == idleFiber->getState()) {
                // 调度器停止了
                FOCUS_LOG_DEBUG(g_logger) << "idle fiber term";
                break;
            }
            ++m_idleThreadCount;
            idleFiber->resume();
            --m_idleThreadCount;
        }
    }
    FOCUS_LOG_DEBUG(g_logger) << "Scheduler::run() exit";
}

void Scheduler::idle() {
    FOCUS_LOG_DEBUG(g_logger) << "idle";
    while(!isCanStop()) {
        Fiber::GetThis()->yield();
    }
}

bool Scheduler::isCanStop() {
    MutexType::Lock lock(m_mutex);
    return m_stopping && m_tasks.empty() && 0 == m_activeThreadCount;
}

void Scheduler::setThis() {
    t_scheduler = this;
}

} // end namespace focus