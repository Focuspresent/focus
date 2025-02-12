#ifndef __FOCUS_TIMER_H__
#define __FOCUS_TIMER_H__

#include <memory>
#include <functional>
#include <cstdint>
#include <set>
#include <vector>
#include "mutex.h"

namespace focus {

class TimerManager;
/**
 * @brief 定时器 毫秒单位
 */
class Timer: public std::enable_shared_from_this<Timer> {
    friend class TimerManager;
public:
    using ptr = std::shared_ptr<Timer>;

    /**
     * @brief 取消定时器
     */
    bool cancel();

    /**
     * @brief 刷新定时器
     */
    bool refresh();

    /**
     * @brief 重载定时器
     * @param[in] ms 周期
     * @param[in] fromNow 是否从现在开始
     */
    bool reset(uint64_t ms, bool fromNow);

private:
    /**
     * @brief 构造函数
     * @param[in] cb 回调函数
     * @param[in] ms 周期
     * @param[in] recurring 是否循环
     * @param[in] manager 管理器
     */
    Timer(std::function<void()> cb, uint64_t ms, bool recurring, TimerManager* manager);

    /**
     * @brief 构造函数
     * @param[in] next 执行的时间
     */
    Timer(uint64_t next);

private:
    /**
     * @brief 定时器比较器
     */
    struct Comparator {
        /**
         * @brief 比较定时器的大小(执行时间)
         * @param[in] lhs 左边的定时器
         * @param[in] rhs 右边的定时器
         */
        bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
    };

private:
    bool m_recurring = false; // 是否循环定时器
    uint64_t m_ms = 0; // 执行周期
    uint64_t m_next = 0; // 执行时间
    std::function<void()> m_cb; // 回调函数
    TimerManager* m_manager = nullptr; // 定时器管理器
};

/**
 * @brief 定时器管理器
 */
class TimerManager {
    friend class Timer;
public:
    using RWMutexType = RWMutex;

    /**
     * @brief 构造函数
     */
    TimerManager();

    /**
     * @brief 析构函数
     */
    virtual ~TimerManager();

    /**
     * @brief 添加定时器
     * @param[in] cb 回调函数
     * @param[in] ms 定时器周期
     * @param[in] recurring 是否循环
     */
    Timer::ptr addTimer(std::function<void()> cb, uint64_t ms, bool recurring = false);

    /**
     * @brief 添加条件定时器
     * @param[in] cb 回调函数
     * @param[in] ms 定时器周期
     * @param[in] cond 条件
     * @param[in] recurring 是否循环
     */
    Timer::ptr addConditionTimer(std::function<void()> cb, uint64_t ms, std::weak_ptr<void> cond, bool recurring = false);

    /**
     * @brief 获取到下一个定时器的间隔
     */
    uint64_t getNextTimer();

    /**
     * @brief 获取需要执行的函数
     * @param[out] cbs 回调函数组
     */
    void listExpiredCb(std::vector<std::function<void()>>& cbs);

    /**
     * @brief 是否有定时器
     */
    bool hasTimer();

protected:
    /**
     * @brief 当有新的定时器插入到首部
     */
    virtual void onTimerInsertAtFront() = 0;

    /**
     * @brief 将定时器添加到管理器中
     */
    void addTimer(Timer::ptr val, RWMutexType::WriteLock& lock);

private:
    /**
     * @brief 检测服务器时间是否被调后了
     */
    bool detectClockRollover(uint64_t nowMs);

private:
    RWMutexType m_mutex; // 读写锁
    std::set<Timer::ptr, Timer::Comparator> m_timers; // 定时器集合
    bool m_tickled = false; // 是否触发首部插入定时器
    uint64_t m_previousTime = 0; // 上一次执行的时间
};

} // end namespace focus

#endif