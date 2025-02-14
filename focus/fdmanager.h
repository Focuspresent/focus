#ifndef __FOCUS_FDMANAGER_H__
#define __FOCUS_FDMANAGER_H__

#include <memory>
#include <cstdint>
#include <vector>
#include "mutex.h"
#include "singleton.h"

namespace focus {

/**
 * @brief 帮助hook模块的fd上下文
 */
class FdCtx: public std::enable_shared_from_this<FdCtx> {
public:
    using ptr = std::shared_ptr<FdCtx>;

    /**
     * @brief 构造函数
     * @param[in] fd 文件句柄
     */
    FdCtx(int fd);

    /**
     * @brief 析构函数
     */
    ~FdCtx();

    /**
     * @brief 是否初始化
     */
    bool isInit() const {
        return m_isInit;
    }

    /**
     * @brief 是否是套接字
     */
    bool isSocket() const {
        return m_isSocket;
    }

    /**
     * @brief 是否关闭
     */
    bool isClose() const {
        return m_isClosed;
    }

    /**
     * @brief 设置hook非阻塞
     */
    void setSysNonblock(bool v) {
        m_sysNonblock = v;
    }

    /**
     * @brief 获取hook非阻塞
     */
    bool getSysNonblock() const {
        return m_sysNonblock;
    }

    /**
     * @brief 设置用户非阻塞
     */
    void setUserNonblock(bool v) {
        m_userNonblock = v;
    }

    /**
     * @brief 获取用户非阻塞
     */
    bool getUserNonblock() const {
        return m_userNonblock;
    }

    /**
     * @brief 设置超时时间
     * @param[in] type 超时时间类型(SO_RCVTIMEO, SO_SNDTIMEO)
     * @param[in] timeout 超时时间(-1不超时)
     */
    void setTimeout(int type, uint64_t timeout);

    /**
     * @brief 获取超时时间
     * @param[in] type 超时时间类型
     */
    uint64_t getTimeout(int type);

private:
    /**
     * @brief 初始化
     */
    bool init();

private:
    bool m_isInit; // 是否初始化
    bool m_isSocket; // 是否是套接字
    bool m_sysNonblock; // 是否hook非阻塞
    bool m_userNonblock; // 是否用户设置非阻塞
    bool m_isClosed; // 是否关闭
    int m_fd; // 句柄
    uint64_t m_recvTimeout; // 读超时时间毫秒
    uint64_t m_sendTimeout; // 写超时时间毫秒
};

/**
 * @brief 句柄管理类
 */
class FdManager {
public:
    using RWMutexType = RWMutex;

    /**
     * @brief 构造函数
     */
    FdManager();

    /**
     * @brief 获取句柄类
     * @param[in] fd 句柄
     * @param[in] autoCreate 是否自动创建
     */
    FdCtx::ptr get(int fd, bool autoCreate = false);

    /**
     * @brief 删除句柄
     * @param[in] fd 句柄
     */
    void del(int fd);

private:
    RWMutexType m_mutex; // 读写锁
    std::vector<FdCtx::ptr> m_fds; // 句柄集合
};

// 单例句柄管理类
using FdMgr = Singleton<FdManager>;

} // end namespace focus

#endif