#include "timer.h"
#include "macro.h"
#include "util.h"

namespace focus {

bool Timer::Comparator::operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const {
    if(!lhs && !rhs) {
        return false;
    }
    if(!lhs) {
        return true;
    }
    if(!rhs) {
        return false;
    }
    if(lhs->m_next < rhs->m_next) {
        return true;
    }
    if(lhs->m_next > rhs->m_next) {
        return false;
    }
    return lhs.get() < rhs.get();
}

bool Timer::cancel() {
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    if(m_cb) {
        m_cb = nullptr;
        auto it = m_manager->m_timers.find(shared_from_this());
        m_manager->m_timers.erase(it);
        return true;
    }
    return false;
}

bool Timer::refresh() {
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    // 没有回调
    if(!m_cb) {
        return false;
    }
    // 没有找到
    auto it = m_manager->m_timers.find(shared_from_this());
    if(m_manager->m_timers.end() == it) {
        return false;
    }
    // 删除并刷新
    m_manager->m_timers.erase(it);
    m_next = GetCurrentMS() + m_ms;
    m_manager->m_timers.insert(shared_from_this());
    return true;
}

bool Timer::reset(uint64_t ms, bool fromNow) {
    // 如果相同，并且不从现在开始
    if(ms == m_ms && !fromNow) {
        return true;
    }
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    if(!m_cb) {
        return false;
    }
    // 没有找到
    auto it = m_manager->m_timers.find(shared_from_this());
    if(m_manager->m_timers.end() == it) {
        return false;
    }
    // 删除并重置
    m_manager->m_timers.erase(it);
    uint64_t start = 0;
    if(fromNow) {
        start = GetCurrentMS();
    }else {
        start = m_next - m_ms;
    }
    m_ms = ms;
    m_next = start + m_ms;
    m_manager->addTimer(shared_from_this(), lock);
    return true;
}

Timer::Timer(std::function<void()> cb, uint64_t ms, bool recurring, TimerManager* manager):
    m_cb(cb), 
    m_ms(ms), 
    m_recurring(recurring),
    m_manager(manager) {
    m_next = GetCurrentMS() + ms;
}

Timer::Timer(uint64_t next):
    m_next(next) {
}

TimerManager::TimerManager() {
    m_previousTime = GetCurrentMS();
}

TimerManager::~TimerManager() {
}

Timer::ptr TimerManager::addTimer(std::function<void()> cb, uint64_t ms, bool recurring) {
    Timer::ptr timer(new Timer(cb, ms, recurring, this));
    RWMutexType::WriteLock lock(m_mutex);
    addTimer(timer, lock);
    return timer;
}

static void OnTimer(std::function<void()> cb, std::weak_ptr<void> cond) {
    std::shared_ptr<void> tmp = cond.lock();
    if(tmp) {
        cb();
    }
}

Timer::ptr TimerManager::addConditionTimer(std::function<void()> cb, uint64_t ms, std::weak_ptr<void> cond, bool recurring) {
    return addTimer(std::bind(&OnTimer, cb, cond), ms, recurring);
}

uint64_t TimerManager::getNextTimer() {
    RWMutexType::ReadLock lock(m_mutex);
    m_tickled = false;
    if(m_timers.empty()) {
        return ~0ull;
    }

    const Timer::ptr& next = *m_timers.begin();
    uint64_t nowMs = GetCurrentMS();
    if(nowMs >= next->m_next) {
        return 0;
    }
    return next->m_next - nowMs;
}

void TimerManager::listExpiredCb(std::vector<std::function<void()>>& cbs) {
    uint64_t nowMs = GetCurrentMS();
    std::vector<Timer::ptr> expired;
    {
        RWMutexType::ReadLock lock(m_mutex);
        if(m_timers.empty()) {
            return ;
        }
    }
    RWMutexType::WriteLock lock(m_mutex);
    if(m_timers.empty()) {
        return ;
    }
    bool rollover = false;
    if(FOCUS_UNLIKELY(detectClockRollover(nowMs))) {
        rollover = true;
    }
    if(!rollover && ((*m_timers.begin())->m_next > nowMs)) {
        return ;
    }

    Timer::ptr nowTimer(new Timer(nowMs));
    auto it = rollover? m_timers.end(): m_timers.lower_bound(nowTimer);
    while(m_timers.end() != it && (*it)->m_next == nowMs) {
        ++it;
    }
    expired.insert(expired.begin(), m_timers.begin(), it);
    m_timers.erase(m_timers.begin(), it);
    cbs.reserve(expired.size());

    for(auto& timer: expired) {
        cbs.emplace_back(timer->m_cb);
        if(timer->m_recurring) {
            timer->m_next = nowMs + timer->m_ms;
            m_timers.insert(timer);
        }else {
            timer->m_cb = nullptr;
        }
    }
}

bool TimerManager::hasTimer() {
    RWMutexType::ReadLock lock(m_mutex);
    return !m_timers.empty();
}

void TimerManager::addTimer(Timer::ptr val, RWMutexType::WriteLock& lock) {
    auto it = m_timers.insert(val).first;
    bool atFront = (m_timers.begin() == it) && !m_tickled;
    if(atFront) {
        m_tickled = true;
    }
    lock.unlock();

    if(atFront) {
        onTimerInsertAtFront();
    }
}

bool TimerManager::detectClockRollover(uint64_t nowMs) {
    bool rollover = false;
    if(nowMs < m_previousTime && nowMs < (m_previousTime - 60 * 60 * 1000)) {
        rollover = true;
    }
    m_previousTime = nowMs;
    return rollover;
}

} // end namespace focus