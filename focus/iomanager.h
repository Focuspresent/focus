#ifndef __FOCUS_IOMANAGER_H__
#define __FOCUS_IOMANAGER_H__

#include <vector>
#include "scheduler.h"
#include "timer.h"

namespace focus {

/**
 * @brief IO调度器
 */
class IOManager: public Scheduler, public TimerManager {
public:
    using ptr = std::shared_ptr<IOManager>;
    using RWMutexType = RWMutex;

    /**
     * @brief IO事件
     * @details 其余事件可以看成读写事件
     */
    enum Event {
        NONE = 0X0, // 没有事件
        READ = 0x1, // 读事件(EPOLLIN)
        WRITE = 0X4 // 写事件(EPOLLOUT)
    };

private:
    /**
     * @brief fd上下文
     */
    struct FdContext {
        using MutexType = Mutex;
        /**
         * @brief 事件上下文
         */
        struct EventContext {
            Scheduler* m_scheduler = nullptr; // 调度器
            Fiber::ptr m_fiber; // 事件协程
            std::function<void()> m_cb; // 事件回调函数
        };

        /**
         * @brief 获取对应事件的上下文
         * @param[in] event 事件类型
         */
        EventContext& getEventContext(Event event);

        /**
         * @brief 重置事件上下文
         * @param[in] ctx 待重置的上下文
         */
        void resetEventContext(EventContext& ctx);

        /**
         * @brief 触发事件
         * @param[in] event 要触发事件的类型
         */
        void triggerEvent(Event event);

        EventContext m_read; // 读事件上下文
        EventContext m_write; // 写事件上下文
        int m_fd = 0; // 事件的文件描述符
        Event m_events = NONE; // 要关心的事件类型
        MutexType m_mutex; // 事件的锁
    };

public:
    /**
     * @brief 构造函数
     * @param[in] threads 工作线程数
     * @param[in] useCaller 是否将当前线程加入调度
     * @param[in] name 调度器的名称
     */
    IOManager(size_t threads = 1, bool useCaller = true, const std::string& name = "IOManager");

    /**
     * @brief 析构函数
     */
    ~IOManager();

    /**
     * @brief 添加事件
     * @details fd发生event事件时执行cb函数
     * @param[in] fd 文件描述符
     * @param[in] event 事件类型
     * @param[in] cb 回调函数
     * @return 0表示成功，-1表示失败
     */
    int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
    
    /**
     * @brief 删除事件
     * @param[in] fd 文件描述符
     * @param[in] event 事件类型
     */
    bool delEvent(int fd, Event event);

    /**
     * @brief 取消事件
     * @param[in] fd 文件描述符
     * @param[in] event 事件类型
     * @attention 如果注册过回调，会触发一次
     */
    bool cancelEvent(int fd, Event event);

    /**
     * @brief 取消所有事件
     * @param[in] fd 文件描述符
     * @attention 在取消前，会触发一次
     */
    bool cancelAll(int fd);

    /**
     * @brief 获取当前的IO调度器
     */
    static IOManager* GetThis();

protected: 
    /**
     * @brief 通知有任务
     */
    void tickle() override;

    /**
     * @brief idle协程
     */
    void idle() override;

    /**
     * @brief 是否可以停止
     */
    bool isCanStop() override;

    /**
     * @brief 是否可以停止，并获取最近一个定时器的超时时间
     * @param[out] timeout 最近定时器的超时时间
     */
    bool isCanStop(uint64_t& timeout);

    /**
     * @brief 有定时器插入首部
     */
    void onTimerInsertAtFront();

    /**
     * @brief 重置上下文容器的大小
     * @param[in] size 重置后的大小
     */
    void resizeContext(size_t size);

private:
    int m_epfd = 0; // epoll文件描述符
    int m_tickleFd[2]; // pipe文件描述符，fd[0]读端，fd[1]写端
    std::atomic<size_t> m_pendingEventCount = {0}; // 当前等待执行的IO事件数量
    RWMutexType m_mutex; // 调度器的锁
    std::vector<FdContext*> m_fdContexts; // fd事件上下文集合
};

} // end namespace focus

#endif