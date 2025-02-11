#ifndef __FOCUS_SCHEDULER_H__
#define __FOCUS_SCHEDULER_H__

#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <list>
#include "log.h"
#include "fiber.h"
#include "thread.h"

namespace focus {

/**
 * @brief 协程调度器
 */
class Scheduler {
public:
    typedef std::shared_ptr<Scheduler> ptr;
    typedef Mutex MutexType;

    /**
     * @brief 创建调度器
     * @param[in] threads 线程数
     * @param[in] useCaller 是否将当前线程也作为调度线程
     * @param[in] name 名称
     */
    Scheduler(size_t threads, bool useCaller = true, const std::string name = "Scheduler");

    /**
     * @brief 析构
     */
    virtual ~Scheduler();

    /**
     * @brief 获取调度器名称
     */
    const std::string getName() const {
        return m_name;
    }

    /**
     * @brief 获取当前线程的调度器 
     */
    static Scheduler* GetThis();

    /**
     * @brief 获取调度协程
     */
    static Fiber* GetMainFiber();

    /**
     * @brief 启动调度器
     */
    void start();

    /**
     * @brief 停止调度器
     */
    void stop();

    /**
     * @brief 添加调度任务
     * @tparam FiberOrCb 调度任务类型
     * @param[in] fc 任务对象
     * @param[in] thread 该任务对象的线程号
     */
    template<class FiberOrCb>
    void schedule(FiberOrCb fc, int thread = -1) {
        bool needTickle = false;
        {
            MutexType::Lock lock(m_mutex);
            needTickle = scheduleNoLock(fc, thread);
        }
        if(needTickle) {
            tickle();
        }
    }

    /**
     * @brief 批量添加调度任务
     * @tparam InputIterator 迭代器类型
     * @param[in] begin 首迭代器
     * @param[in] end 尾迭代器
     */
    template<class InputIterator>
    void schedule(InputIterator begin, InputIterator end) {
        bool needTickle = false;
        {
            MutexType::Lock lock(m_mutex);
            while(begin != end) {
                needTickle = scheduleNoLock(*begin, -1) || needTickle;
                ++begin;
            }
        }
        if(needTickle) {
            tickle();
        }
    }

protected:
    /**
     * @brief 通知有任务
     */
    virtual void tickle();

    /**
     * @brief 运行调度器
     */
    void run();

    /**
     * @brief 没有任务时执行idle协程
     */
    virtual void idle();

    /**
     * @brief 是否可以停止
     */
    virtual bool isCanStop();

    /**
     * @brief 设置协程调度器
     */
    void setThis();

    /**
     * @brief 是否有空闲的线程
     */
    bool hasIdleThreads() {
        return m_idleThreadCount > 0;
    }

private:
    /**
     * @brief 无锁，添加调度任务
     * @tparam FiberOrCb 调度任务类型
     * @param[in] fc 任务对象
     * @param[in] thread 该任务的线程号，-1表示任意线程
     */
    template<class FiberOrCb>
    bool scheduleNoLock(FiberOrCb fc, int thread) {
        // 是否需要通知
        bool needTickle = m_tasks.empty();
        // 创建一个任务
        ScheduleTask task(fc, thread);
        // 任务实际对象不空
        if(task.m_fiber || task.m_cb) {
            m_tasks.emplace_back(task);
        }
        return needTickle;
    }   

private:
    /**
     * @brief 调度任务，协程或者函数，并且可以绑定线程
     */
    struct ScheduleTask {
        Fiber::ptr m_fiber; // 协程对象
        std::function<void()> m_cb; // 函数
        int m_thread; // 线程id

        /**
         * @brief 无参构造
         */
        ScheduleTask() {
            m_thread = -1;
        }

        /**
         * @brief 构造函数
         * @param[in] f 协程对象
         * @param[in] thread 线程id
         */
        ScheduleTask(Fiber::ptr f, int thread) {
            m_fiber = f;
            m_thread = thread;
        }

        /**
         * @brief 构造函数
         * @param[in] f 协程对象指针
         * @param[in] thread 线程id
         */
        ScheduleTask(Fiber::ptr* f, int thread) {
            m_fiber.swap(*f);
            m_thread = thread;
        }

        /**
         * @brief 构造函数
         * @param[in] cb 函数对象
         * @param[in] thread 线程id
         */
        ScheduleTask(std::function<void()> cb, int thread) {
            m_cb = cb;
            m_thread = thread;
        }

        /**
         * @brief 构造函数
         * @param[in] cb 函数对象指针
         * @param[in] thread 线程id
         */
        ScheduleTask(std::function<void()>* cb, int thread) {
            m_cb.swap(*cb);
            m_thread = thread;
        }

        /**
         * @brief 重置对象
         */
        void reset() {
            m_fiber = nullptr;
            m_cb = nullptr;
            m_thread = -1;
        }
    };

private:
    std::string m_name; // 协程调度器名称
    MutexType m_mutex; // 互斥锁
    std::vector<Thread::ptr> m_threads; // 线程池
    std::list<ScheduleTask> m_tasks; // 任务队列
    std::vector<int> m_threadIds; // 线程池的线程id数组
    size_t m_threadCount = 0; // 工作线程数量，不包含use_caller的主线程
    std::atomic<size_t> m_activeThreadCount = {0}; // 活跃线程数
    std::atomic<size_t> m_idleThreadCount = {0}; // 空闲线程数

    bool m_useCaller; // 是否use caller
    Fiber::ptr m_rootFiber; // use_caller为true时,调度器所在线程的调度协程
    int m_rootThread = 0; // use_caller为true时,调度器所在线程的id

    bool m_stopping = false; // 是否正在停止
};

} // end namespace focus

#endif