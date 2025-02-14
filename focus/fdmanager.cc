#include "fdmanager.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

namespace focus {

FdCtx::FdCtx(int fd):
    m_isInit(false),
    m_isSocket(false),
    m_sysNonblock(false),
    m_userNonblock(false),
    m_isClosed(false),
    m_fd(fd),
    m_recvTimeout(-1),
    m_sendTimeout(-1) {
    init();
}

FdCtx::~FdCtx() {
}

void FdCtx::setTimeout(int type, uint64_t timeout) {
    if(SO_RCVTIMEO == type) {
        m_recvTimeout = timeout;
    }else {
        m_sendTimeout = timeout;
    }
}

uint64_t FdCtx::getTimeout(int type) {
    if(SO_RCVTIMEO == type) {
        return m_recvTimeout;
    }else {
        return m_sendTimeout;
    }
}

bool FdCtx::init() {
    // 初始化过
    if(m_isInit) {
        return true;
    }
    // 都不超时
    m_recvTimeout = -1;
    m_sendTimeout = -1;

    struct stat fdStat;
    if(-1 == fstat(m_fd, &fdStat)) {
        // 读取失败
        m_isInit = false;
        m_isSocket = false;
    }else {
        // 读取成功
        m_isInit = true;
        // 判断是否是套接字
        m_isSocket = S_ISSOCK(fdStat.st_mode);
    }

    // 如果是套接字 TODO
    if(m_isSocket) {
        int flags = fcntl(m_fd, F_GETFL, 0);
        // 如果没有设置非阻塞
        if(!(flags & O_NONBLOCK)) {
            fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);
        }
        m_sysNonblock = true;
    }else {
        m_sysNonblock = false;
    }

    // 默认用户未设置
    m_userNonblock = false;
    // 还没关闭
    m_isClosed = false;
    return m_isInit;
}

FdManager::FdManager() {
    m_fds.resize(64);
}

FdCtx::ptr FdManager::get(int fd, bool autoCreate) {
    if(-1 == fd) {
        return nullptr;
    }
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fds.size() <= fd) {
        // 不够，并且不自动创建
        if(!autoCreate) {
            return nullptr;
        }
    }else {
        // 存在，或者不自动创建
        if(m_fds[fd] || !autoCreate) {
            return m_fds[fd];
        }
    }
    lock.unlock();

    // 扩容
    RWMutexType::WriteLock lock2(m_mutex);
    FdCtx::ptr ctx(new FdCtx(fd));
    if(fd >= (int)m_fds.size()) {
        m_fds.resize(fd * 1.5);
    }
    m_fds[fd] = ctx;
    return ctx;
}

void FdManager::del(int fd) {
    RWMutexType::WriteLock lock(m_mutex);
    if((int)m_fds.size() <= fd) {
        return ;
    }
    m_fds[fd].reset();
}

}