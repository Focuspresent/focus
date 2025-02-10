#ifndef __FOCUS_UTIL_H__
#define __FOCUS_UTIL_H__

#include <cxxabi.h>
#include <pthread.h>
#include <unistd.h>
#include <cstdint>
#include <sys/syscall.h>
#include <vector>
#include <string>

namespace focus{

// 获取线程id
pid_t GetThreadId();

// 获取协程id TODO
uint64_t GetFiberId();

/**
 * @brief 获取当前调用栈
 * @param[out] bt 保存调用栈
 * @param[in] size 最多返回层数
 * @param[in] skip 跳过栈顶的层数
 */
void Backtrace(std::vector<std::string>& bt, int size = 64, int skip = 1);

/**
 * @brief 获取当前栈信息的字符串
 * @param[in] size 栈的最大层数
 * @param[in] skip 跳过栈顶的层数
 * @param[in] prefix 栈信息前输出的内容
 */
std::string BacktraceToString(int size = 64, int skip = 2, const std::string& prefix = "");

// 可视化c++类型名
template <class T>
const char* TypeToName() {
    const char* name=__cxxabiv1::__cxa_demangle(typeid(T).name(),nullptr,nullptr,nullptr);
    return name;
}

}

#endif