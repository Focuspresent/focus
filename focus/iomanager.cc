#include "iomanager.h"
#include "macro.h"
#include <sys/epoll.h>
#include <fcntl.h>

namespace focus {

// 全局日志器
static Logger::ptr g_logger = FOCUS_LOG_NAME("system");

enum EPOLL_CTL_OP_TYPE {
};

// 重载epoll操作输出
static std::ostream& operator<<(std::ostream& os, const EPOLL_CTL_OP_TYPE& op) {
    switch((int)op) {
#define XX(ctl)         \
    case ctl:           \
        return os << #ctl;
        XX(EPOLL_CTL_ADD);
        XX(EPOLL_CTL_MOD);
        XX(EPOLL_CTL_DEL);
#undef XX
        default:
            return os << (int)op;
    }
}

// 重载epoll事件类型输出
static std::ostream& operator<<(std::ostream& os, EPOLL_EVENTS events) {
    if(!events) {
        return os << "0";
    }
    bool first = true;
#define XX(E)           \
    if(events & E) {    \
        if(!first) {    \
            os << "|";  \
        }               \
        os << #E;       \
        first = false;  \
    }                   
    XX(EPOLLIN);
    XX(EPOLLPRI);
    XX(EPOLLOUT);
    XX(EPOLLRDNORM);
    XX(EPOLLRDBAND);
    XX(EPOLLWRNORM);
    XX(EPOLLWRBAND);
    XX(EPOLLMSG);
    XX(EPOLLERR);
    XX(EPOLLHUP);
    XX(EPOLLRDHUP);
    XX(EPOLLONESHOT);
    XX(EPOLLET);
#undef XX
    return os;
}

IOManager::FdContext::EventContext& IOManager::FdContext::getEventContext(Event event) {
    switch(event) {
        case IOManager::READ:
            return m_read;
        case IOManager::WRITE:
            return m_write;
        default:
            FOCUS_ASSERT2(false, "getEventContext");
    }
    throw std::invalid_argument("getEventContext invalid event");
}

void IOManager::FdContext::resetEventContext(EventContext& ctx) {
    ctx.m_scheduler = nullptr;
    ctx.m_fiber.reset();
    ctx.m_cb = nullptr;
}

void IOManager::FdContext::triggerEvent(Event event) {
    // 必须注册过
    FOCUS_ASSERT(m_events & event);
    // 清除事件，只会触发一次
    m_events = (Event)(m_events & ~event);
    // 调度协程
    EventContext& ctx = getEventContext(event);
    if(ctx.m_cb) {
        ctx.m_scheduler->schedule(ctx.m_cb);
    }else {
        ctx.m_scheduler->schedule(ctx.m_fiber);
    }
    resetEventContext(ctx);
    return ;
}

IOManager::IOManager(size_t threads, bool useCaller, const std::string& name):
    Scheduler(threads, useCaller, name) {
    // 创建epoll句柄
    m_epfd = epoll_create(1);
    // 判断epoll句柄
    FOCUS_ASSERT(m_epfd > 0);

    // 创建管道
    int rt = pipe(m_tickleFd);
    // 判断是否成功
    FOCUS_ASSERT(!rt);

    // 关注管道读句柄的可读事件，用于通知协程
    epoll_event event;
    memset(&event, 0, sizeof(epoll_event));
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_tickleFd[0];

    // 非阻塞
    rt = fcntl(m_tickleFd[0], F_SETFL, O_NONBLOCK);
    // 判断是否设置成功
    FOCUS_ASSERT(!rt);

    // 使用epoll关注管道读句柄的可读事件
    rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFd[0], &event);
    // 判断是否添加成功
    FOCUS_ASSERT(!rt);

    // 设置fd上下文集合
    resizeContext(32);

    // 启动调度器
    start();
}

IOManager::~IOManager() {
    // 停止调度器
    stop();
    // 关闭相关句柄
    close(m_epfd);
    close(m_tickleFd[0]);
    close(m_tickleFd[1]);

    // 释放内存
    for(size_t i = 0; i < m_fdContexts.size(); ++i) {
        if(m_fdContexts[i]) {
            delete m_fdContexts[i];
            m_fdContexts[i] = nullptr;
        }
    }
}

int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
    // 找到fd对应的fdContext
    FdContext* fdCtx = nullptr;
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() > fd) {
        // 足够
        fdCtx = m_fdContexts[fd];
        lock.unlock();
    }else {
        lock.unlock();
        // 扩容
        RWMutexType::WriteLock lock2(m_mutex);
        resizeContext(fd * 1.5);
        fdCtx = m_fdContexts[fd];
    }

    // 同一个fd不可以重复添加相同的事件
    FdContext::MutexType::Lock lock2(fdCtx->m_mutex);
    if(FOCUS_UNLIKELY(fdCtx->m_events & event)) {
        FOCUS_LOG_ERROR(g_logger) << "addEvent assert fd = " << fd
                                  << " event = " << (EPOLL_EVENTS)event
                                  << " fdCtx.event = " << (EPOLL_EVENTS)fdCtx->m_events;
        FOCUS_ASSERT(!(fdCtx->m_events & event));
    }

    // 构造epoll_event
    int op = fdCtx->m_events? EPOLL_CTL_MOD: EPOLL_CTL_ADD;
    epoll_event epevent;
    epevent.events = EPOLLET | fdCtx->m_events | event;
    epevent.data.ptr = fdCtx;

    // 添加新的事件
    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        FOCUS_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd <<", "
                                  << (EPOLL_CTL_OP_TYPE)op <<", " << fd <<", " << (EPOLL_EVENTS)epevent.events <<"):"
                                  << rt << " (" << errno << ") (" << strerror(errno) << ") fdCtx->events = "
                                  << (EPOLL_EVENTS)fdCtx->m_events;
        return -1;
    }

    // 增加一个待执行IO事件
    ++m_pendingEventCount;

    // 找到fd对应的EventContext，并赋值
    fdCtx->m_events = (Event)(fdCtx->m_events | event);
    FdContext::EventContext& eventCtx = fdCtx->getEventContext(event);
    // 都是空结束
    FOCUS_ASSERT(!eventCtx.m_scheduler && !eventCtx.m_fiber && !eventCtx.m_cb);

    // 赋值
    eventCtx.m_scheduler = Scheduler::GetThis();
    if(cb) {
        eventCtx.m_cb.swap(cb);
    }else {
        eventCtx.m_fiber = Fiber::GetThis();
        FOCUS_ASSERT2(Fiber::RUNNING == eventCtx.m_fiber->getState(), "state = " << eventCtx.m_fiber->getState());
    }

    return 0;
}

bool IOManager::delEvent(int fd, Event event) {
    // 找到fd对应的FdContext
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext* fdCtx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fdCtx->m_mutex);
    if(FOCUS_UNLIKELY(!(fdCtx->m_events & event))) {
        return false;
    }

    // 清除指定的事件
    Event newEvents = (Event)(fdCtx->m_events & ~event);
    int op = newEvents? EPOLL_CTL_MOD: EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | newEvents;
    epevent.data.ptr = fdCtx;

    // 操作epoll
    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        FOCUS_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                  << (EPOLL_CTL_OP_TYPE)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events <<"): "
                                  << rt << "(" << errno << ") (" << strerror(errno) << ")";
        return false;   
    }

    // 减少待执行事件
    --m_pendingEventCount;
    
    // 重置fd对应的事件上下文
    fdCtx->m_events = newEvents;
    FdContext::EventContext& eventCtx = fdCtx->getEventContext(event);
    fdCtx->resetEventContext(eventCtx);
    return true;
}

bool IOManager::cancelEvent(int fd, Event event) {
    // 找到fd对应的FdContext
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext* fdCtx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fdCtx->m_mutex);
    if(FOCUS_UNLIKELY(!(fdCtx->m_events & event))) {
        return false;
    }

    // 构造epool_event
    Event newEvents = (Event)(fdCtx->m_events & ~event);
    int op = newEvents? EPOLL_CTL_MOD: EPOLL_CTL_ADD;
    epoll_event epevent;
    epevent.events = EPOLLET | newEvents;
    epevent.data.ptr = fdCtx;

    // 删除事件
    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        FOCUS_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                  << (EPOLL_CTL_OP_TYPE)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
                                  << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    // 删除之前触发一次事件
    fdCtx->triggerEvent(event);
    // 减少执行事件
    --m_pendingEventCount;
    return true;
}

bool IOManager::cancelAll(int fd) {
    // 找到fd对应的FdContext
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext* fdCtx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fdCtx->m_mutex);
    if(!fdCtx->m_events) {
        return false;
    }

    // 构造epoll_event
    int op = EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = 0;
    epevent.data.ptr = fdCtx;

    //删除全部事件
    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        FOCUS_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                  << (EPOLL_CTL_OP_TYPE)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
                                  << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    // 触发注册的读事件
    if(fdCtx->m_events & READ) {
        fdCtx->triggerEvent(READ);
        --m_pendingEventCount;
    }

    // 触发注册的写事件
    if(fdCtx->m_events & WRITE) {
        fdCtx->triggerEvent(WRITE);
        --m_pendingEventCount;
    }

    // 保证已经触发完成
    FOCUS_ASSERT(0 == fdCtx->m_events);
    return true;
}

IOManager* IOManager::GetThis() {
    // 动态转型
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle() {
    FOCUS_LOG_DEBUG(g_logger) << "tickle";
    // 没有空闲线程
    if(!hasIdleThreads()) {
        return ;
    }
    // 唤醒
    int rt = write(m_tickleFd[1], "T", 1);
    FOCUS_ASSERT(1 == rt);
}

void IOManager::idle() {
    FOCUS_LOG_DEBUG(g_logger) << "idle";

    // 等待事件
    const uint64_t MAX_EVENTS = 256;
    epoll_event* events = new epoll_event[MAX_EVENTS]();
    std::shared_ptr<epoll_event> shared_events(events, [](epoll_event* ptr){
        delete[] ptr;
    });

    // 循环
    while(true) {
        // 判断调度器是否停止
        uint64_t nextTimeout = 0;
        if(FOCUS_UNLIKELY(isCanStop(nextTimeout))) {
            FOCUS_LOG_DEBUG(g_logger) << "name = "<< getName() <<" idle exit";
            break;
        }

        // 等待事件发生或者超时
        int rt = 0;
        do {
            static const int MAX_TIMEOUT = 5000;
            if(~0ull != nextTimeout) {
                nextTimeout = std::min((int)nextTimeout, MAX_TIMEOUT);
            }else {
                nextTimeout = MAX_TIMEOUT;
            } 
            rt = epoll_wait(m_epfd, events, MAX_EVENTS, (int)nextTimeout);
            // 为了确保中断信号不会导致程序退出或停止等待
            if(rt < 0 && errno == EINTR) {
                continue;
            }else {
                break;
            }
        }while(true);

        // 获取超时的定时器，执行函数
        std::vector<std::function<void()>> cbs;
        listExpiredCb(cbs);
        if(!cbs.empty()) {
            for(const auto& cb: cbs) {
                schedule(cb);
            }
            cbs.clear();
        }

        // 遍历所有发生的事件
        for(int i = 0; i < rt; ++i) {
            epoll_event& event = events[i];
            if(event.data.fd == m_tickleFd[0]) {
                // ticklefd[0] 用于通知协程
                uint8_t dummy[256];
                while(read(m_tickleFd[0], dummy, sizeof(dummy)) > 0);
                continue;
            }

            FdContext* fdCtx = (FdContext*)event.data.ptr;
            FdContext::MutexType::Lock lock(fdCtx->m_mutex);

            // 获取实际事件
            // 这两种错误都要触发读和写事件
            if(event.events & (EPOLLERR | EPOLLHUP)) {
                event.events |= (EPOLLIN | EPOLLOUT) & fdCtx->m_events;
            }
            int realEvents = NONE;
            if(event.events & EPOLLIN) {
                realEvents |= READ;
            }
            if(event.events & EPOLLOUT) {
                realEvents |= WRITE;
            }

            if(NONE == (fdCtx->m_events & realEvents)) {
                continue;
            }

            // 获取剩余的事件
            int leftEvents = (fdCtx->m_events & ~realEvents);
            int op = leftEvents? EPOLL_CTL_MOD: EPOLL_CTL_DEL;
            event.events = EPOLLET | leftEvents;

            // 添加剩余的事件
            int rt2 = epoll_ctl(m_epfd, op, fdCtx->m_fd, &event);
            if(rt2) {
                FOCUS_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                                  << (EPOLL_CTL_OP_TYPE)op << ", " << fdCtx->m_fd << ", " << (EPOLL_EVENTS)event.events << "):"
                                  << rt2 << " (" << errno << ") (" << strerror(errno) << ")";
                continue;
            }

            // 处理已经发生的事件
            if(realEvents & READ) {
                fdCtx->triggerEvent(READ);
                --m_pendingEventCount;
            }
            if(realEvents & WRITE) {
                fdCtx->triggerEvent(WRITE);
                --m_pendingEventCount;
            }
        }

        // 一旦处理完所有的事件，idle协程yield，这样可以让调度协程(Scheduler::run)重新检查是否有新任务要调度
        Fiber::ptr cur = Fiber::GetThis();
        auto raw_ptr   = cur.get();
        cur.reset();

        raw_ptr->yield();
    }
}

bool IOManager::isCanStop() {
    uint64_t timeout = 0;
    return isCanStop(timeout);
}

bool IOManager::isCanStop(uint64_t& timeout) {
    // 等待所有IO事件 确保没有剩余的定时器
    timeout = getNextTimer();
    return ~0ull == timeout && 0 == m_pendingEventCount && Scheduler::isCanStop();
}

void IOManager::onTimerInsertAtFront() {
    tickle();
}

void IOManager::resizeContext(size_t size) {
    m_fdContexts.resize(size);

    for(size_t i = 0; i < m_fdContexts.size(); ++i) {
        if(!m_fdContexts[i]) {
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i]->m_fd = i;
        }
    }
}

} // end namespace focus