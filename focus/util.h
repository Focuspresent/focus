#ifndef __FOCUS_UTIL_H__
#define __FOCUS_UTIL_H__

#include <cxxabi.h>

namespace focus{

// 可视化c++类型名
template <class T>
const char* TypeToName() {
    const char* name=__cxxabiv1::__cxa_demangle(typeid(T).name(),nullptr,nullptr,nullptr);
    return name;
}

}

#endif