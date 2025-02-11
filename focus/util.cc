#include "util.h"
#include "log.h"
#include "fiber.h"
#include <execinfo.h>
#include <sstream>
#include <sys/time.h>

namespace focus {

// 全局日志器
static Logger::ptr g_logger = FOCUS_LOG_NAME("system");

pid_t GetThreadId() {
    return syscall(SYS_gettid);
}

std::string GetThreadName() {
    char thread_name[16] = {0};
    pthread_getname_np(pthread_self(), thread_name, 16);
    return std::string(thread_name);
}

void SetThreadName(const std::string& name) {
    pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
}

uint64_t GetFiberId() {
    return Fiber::GetFiberId();
}

uint64_t GetCurrentMS() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000ul  + tv.tv_usec / 1000;
}

// 将编译器读取的函数名编码转成看得懂的
static std::string demangle(const char* str) {
    size_t size = 0;
    int status = 0;
    std::string rt;
    rt.resize(256);
    // 使用api
    if(1 == sscanf(str, "%*[^(]%*[^_]%255[^)+]", &rt[0])) {
        char* v = abi::__cxa_demangle(&rt[0], nullptr, &size, &status);
        if(v) {
            std::string result(v);
            free(v);
            return result;
        }
    }

    // 直接原始字符串
    if(1 == sscanf(str, "%255s", &rt[0])) {
        return rt;
    }
    return str;
}

void Backtrace(std::vector<std::string>& bt, int size, int skip) {
    void** array = (void**)malloc((sizeof(void*) * size));
    size_t s = ::backtrace(array, size);

    char** strings = backtrace_symbols(array, s);
    if(NULL == strings) {
        FOCUS_LOG_ERROR(g_logger) << "backtrace_symbols error";
        return ;
    }

    for(size_t i = skip; i < s; ++i) {
        bt.emplace_back(demangle(strings[i]));
    }

    free(strings);
    free(array);
}

std::string BacktraceToString(int size, int skip, const std::string& prefix) {
    std::vector<std::string> bt;
    Backtrace(bt, size, skip);
    std::stringstream ss;
    for(size_t i = 0; i < bt.size(); ++i) {
        ss << prefix << bt[i] << std::endl;
    }
    return ss.str();
}

}