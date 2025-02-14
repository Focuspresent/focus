#include "hook.h"
#include "log.h"
#include "config.h"
#include "fdmanager.h"
#include "iomanager.h"
#include "macro.h"
#include <dlfcn.h>

// 全局日志器
focus::Logger::ptr g_logger = FOCUS_LOG_NAME("system");

namespace focus {

// tcp超时时间(默认5秒)
static ConfigVar<int>::ptr g_tcp_connect_timeout = 
    Config::LookUp("tcp.connect.timeout", 5000, "tcp connect timeout");

// 线程局部变量
static thread_local bool t_hook_enable = false;

// 对hook的系统调用进行xx操作
#define HOOK_FUN(XX) \
    XX(sleep)        \
    XX(usleep)       \
    XX(nanosleep)    \
    XX(socket)       \
    XX(connect)      \
    XX(accept)       \
    XX(read)         \
    XX(readv)        \
    XX(recv)         \
    XX(recvfrom)     \
    XX(recvmsg)      \
    XX(write)        \
    XX(writev)       \
    XX(send)         \
    XX(sendto)       \
    XX(sendmsg)      \
    XX(close)        \
    XX(fcntl)        \
    XX(ioctl)        \
    XX(getsockopt)   \
    XX(setsockopt)   

// hook初始化
void hookInit() {
    // 保证初始化一次
    static bool isInited = false;
    if(isInited) {
        return ;
    }
    isInited = true;
    // 存储原先(未hook)的函数指针
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX)
#undef XX
}

static uint64_t s_connect_timeout = -1;
struct HookIniter {
    HookIniter() {
        // 初始化hook
        hookInit();

        // 获取连接超时时间
        s_connect_timeout = g_tcp_connect_timeout->getVal();

        // 绑定回调函数
        g_tcp_connect_timeout->addCallBack([](const int& oldVal, const int& newVal){
            FOCUS_LOG_INFO(g_logger) << "tcp connect timeout change from "
                                     << oldVal << " to " << newVal;
            s_connect_timeout = newVal;
        });
    }
};

// 初始化hook
static HookIniter s_hook_initer;

bool isHookEnable() {
    return t_hook_enable;
}

void setHookEnable(bool flag) {
    t_hook_enable = flag;
}

} // end namespace focus

/**
 * @brief 定时器是否取消
 */
struct TimerInfo {
    int cancelled = 0;
};

/**
 * @brief IO相关系统调用的执行模版函数
 * @tparam OriginFun 未hook的系统调用函数指针
 * @tparam Args 可变参数
 * @param[in] fd 句柄
 * @param[in] fun 执行函数
 * @param[in] hookFunName hook的系统调用名
 * @param[in] event 事件类型
 * @param[in] timeoutType 超时类型
 * @param[in] args 可变参数包
 */
template<typename OriginFun, typename... Args>
static ssize_t doIo(int fd, OriginFun fun, const char* hookFunName, uint32_t event, int timeoutType, Args&&... args) {
    // 没有设置hook
    if(!focus::t_hook_enable) {
        return fun(fd, std::forward<Args>(args)...);
    }

    // 没有在fd管理类中找到fd
    focus::FdCtx::ptr ctx = focus::FdMgr::GetInstance()->get(fd);
    if(!ctx) {
        return fun(fd, std::forward<Args>(args)...);
    }

    // 如果已经关闭了
    if(ctx->isClose()) {
        errno = EBADF; // 表示句柄无效或者关闭
        return -1;
    }

    // 如果不是套接字或者用户显示设置了非阻塞
    if(!ctx->isSocket() || ctx->getUserNonblock()) {
        return fun(fd, std::forward<Args>(args)...);
    } 

    // 获取超时时间
    uint64_t to = ctx->getTimeout(timeoutType);
    std::shared_ptr<TimerInfo> tinfo(new TimerInfo);

retry:
    // 调用实际执行函数
    ssize_t n = fun(fd, std::forward<Args>(args)...);

    // 如果是由中断导致的调用失败，就再次尝试
    while(-1 == n && EINTR == errno) {
        n = fun(fd, std::forward<Args>(args)...);
    }

    // 如果是由资源不可用导致的失败
    if(-1 == n && EAGAIN == errno) {
        focus::IOManager* iom = focus::IOManager::GetThis();
        focus::Timer::ptr timer;
        std::weak_ptr<TimerInfo> winfo(tinfo);

        // 设置了超时时间
        if((uint64_t)-1 != to) {
            timer = iom->addConditionTimer([winfo, fd, iom, event](){
                auto t =winfo.lock();
                // Timerinfo对象无效或者取消
                if(!t || t->cancelled) {
                    return ;
                }
                // 超时，并且还未处理
                t->cancelled = ETIMEDOUT;
                // 取消事件，并触发一次
                iom->cancelEvent(fd, (focus::IOManager::Event)(event));
            },to,winfo);
        }

        // 在fd上添加事件
        int rt = iom->addEvent(fd, (focus::IOManager::Event)(event));
        if(FOCUS_UNLIKELY(rt)) {
            // 添加失败
            FOCUS_LOG_ERROR(g_logger) << hookFunName 
                                      << " addEvent(" << fd << ", "
                                      << event << ")";
            // 取消定时器
            if(timer) {
                timer->cancel();
            }
            return -1;
        }else {
            // 让出执行权，等待事件
            focus::Fiber::GetThis()->yield();
            // 恢复后，取消先前的定时器
            if(timer) {
                timer->cancel();
            }
            // 如果先前被设置了超时，就是因为超时导致的失败
            if(tinfo->cancelled) {
                errno = tinfo->cancelled;
                return -1;
            }
            // 没有超时，重新尝试对应操作
            goto retry;
        }
    }

    return n;
}

extern "C" {
#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX)
#undef XX

unsigned int sleep(unsigned int seconds) {
    // 没有hook
    if(!focus::t_hook_enable) {
        return sleep_f(seconds);
    }

    focus::Fiber::ptr fiber = focus::Fiber::GetThis();
    focus::IOManager* iom = focus::IOManager::GetThis();

    // 添加定时器，等待调度
    iom->addTimer(std::bind((void(focus::Scheduler::*)(focus::Fiber::ptr, int thread))&focus::IOManager::schedule, iom, fiber, -1),
                seconds * 1000);

    // 让出执行权
    focus::Fiber::GetThis()->yield();

    return 0;
}

int usleep(useconds_t usec) {
    // 没有hook
    if(!focus::t_hook_enable) {
        return usleep_f(usec);
    }

    focus::Fiber::ptr fiber = focus::Fiber::GetThis();
    focus::IOManager* iom = focus::IOManager::GetThis();

    // 添加定时器，等待调度
    iom->addTimer(std::bind((void(focus::Scheduler::*)(focus::Fiber::ptr, int thread))&focus::IOManager::schedule, iom, fiber, -1),
                usec / 1000);

    // 让出执行权
    focus::Fiber::GetThis()->yield();

    return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
    // 没有hook
    if(!focus::t_hook_enable) {
        return nanosleep_f(req, rem);
    }

    int timeoutMs = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
    focus::Fiber::ptr fiber = focus::Fiber::GetThis();
    focus::IOManager* iom = focus::IOManager::GetThis();

    // 添加定时器，等待调度
    iom->addTimer(std::bind((void(focus::Scheduler::*)(focus::Fiber::ptr, int thread))&focus::IOManager::schedule, iom, fiber, -1),
                timeoutMs);

    // 让出执行权
    focus::Fiber::GetThis()->yield();

    return 0;
}

int socket(int domain, int type, int protocol) {
    // 没有hook
    if(!focus::t_hook_enable) {
        return socket_f(domain, type, protocol);
    }

    // 创建套接字
    int fd = socket_f(domain, type, protocol);
    if(-1 == fd) {
        return fd;
    }

    // 创建套接字上下文
    focus::FdMgr::GetInstance()->get(fd, true);
    
    return fd;
}

int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeoutMs) {
    // TODO
    return -1;
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    return connect_with_timeout(sockfd, addr, addrlen, focus::s_connect_timeout);
}

int accept(int s, struct sockaddr* addr, socklen_t* addrlen) {
    int fd = doIo(s, accept_f, "accept", focus::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if(fd >= 0) {
        focus::FdMgr::GetInstance()->get(fd, true);
    }
    return fd;
}

ssize_t read(int fd, void* buf, size_t count) {
    return doIo(fd, read_f, "read", focus::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec* iov, int iovcnt) {
    return doIo(fd, readv_f, "readv", focus::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
    return doIo(sockfd, recv_f, "recv", focus::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* srcaddr, socklen_t* addrlen) {
    return doIo(sockfd, recvfrom_f, "recvfrom", focus::IOManager::READ, SO_RCVTIMEO, buf, len, flags, srcaddr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
    return doIo(sockfd, recvmsg_f, "recvmsg", focus::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void* buf, size_t count) {
    return doIo(fd, write_f, "write", focus::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
    return doIo(fd, writev_f, "writev", focus::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int s, const void* msg, size_t len, int flags) {
    return doIo(s, send_f, "send", focus::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int s, const void* msg, size_t len, int flags, const struct sockaddr* to, socklen_t tolen) {
    return doIo(s, sendto_f, "sendto", focus::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr* msg, int flags) {
    return doIo(s, sendmsg_f, "sendmsg", focus::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd) {
    // 没有hook
    if(!focus::t_hook_enable) {
        return close_f(fd);
    }

    // 获取句柄上下文
    focus::FdCtx::ptr ctx = focus::FdMgr::GetInstance()->get(fd);
    // 处理句柄上下文
    if(ctx) {   
        auto iom = focus::IOManager::GetThis();
        // 取消相关事件，并触发
        if(iom) {
            iom->cancelAll(fd);
        }
        // 从fd管理中删除
        focus::FdMgr::GetInstance()->del(fd);
    }
    
    // 关闭句柄
    return close_f(fd);
}

int fcntl(int fd, int cmd, ...) {
    // TODO
    return -1;
}

int ioctl(int d, unsigned long int request, ...) {
    // TODO
    return -1;
}

int getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
    // TODO
    return -1;
}

} // end extern "C"