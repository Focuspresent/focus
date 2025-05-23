#include "thread.h"
#include "log.h"

namespace focus {

static thread_local Thread* t_thread=nullptr; // 当前线程指针
static thread_local std::string t_thread_name="UNKNOWN"; //当前线程名

static Logger::ptr g_logger=FOCUS_LOG_NAME("system"); // system日志器

Thread* Thread::GetThis() {
    return t_thread;
}

const std::string& Thread::GetName() {
    return t_thread_name;
}

void Thread::SetName(const std::string& name) {
    if(name.empty()){
        return ;
    }
    if(nullptr!=t_thread){
        t_thread->m_name=name;
    }
    t_thread_name=name;
}

Thread::Thread(std::function<void()> cb,const std::string& name):
    m_cb(cb),
    m_name(name) {
    if(name.empty()){
        m_name="UNKNOWN";
    }
    int rt=pthread_create(&m_thread,nullptr,&Thread::run,this);
    if(rt){
        FOCUS_LOG_ERROR(g_logger)<<"pthread_create thread fail, rt="<<rt
            <<" name="<<m_name;
        throw std::logic_error("pthread_create fail");
    }
    m_semaphore.wait();
}

Thread::~Thread() {
    if(m_thread){
        pthread_detach(m_thread);
    }
}

void Thread::join() {
    if(m_thread){
        int rt=pthread_join(m_thread,nullptr);
        if(rt){
            FOCUS_LOG_ERROR(g_logger)<<"pthread_join thread fail, rt="<<rt
                <<" name="<<m_name;
            throw std::logic_error("pthread_join fail");
        }
        m_thread=0;
    }
}

void* Thread::run(void* arg) {
    Thread* thread=(Thread*)arg;
    t_thread=thread;
    t_thread_name=thread->m_name;
    thread->m_id=GetThreadId();
    pthread_setname_np(thread->m_thread,thread->m_name.substr(0,15).c_str());

    std::function<void()> cb;
    cb.swap(thread->m_cb);

    thread->m_semaphore.notify();

    cb();
    return nullptr;
}

}