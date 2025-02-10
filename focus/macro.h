#ifndef __FOCUS_MACRO_H__
#define __FOCUS_MACRO_H__

#include <string.h>
#include <assert.h>
#include "log.h"
#include "util.h"

#if defined __GNUC__ || defined __llvm__
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率成立
#define FOCUS_LIKELY(x) __builtin_expect(!!(x), 1)
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率不成立
#define FOCUS_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define FOCUS_LIKELY(x) (x)
#define FOCUS_UNLIKELY(x) (x)
#endif

/// 断言宏封装
#define FOCUS_ASSERT(x)                                                                  \
    if(FOCUS_UNLIKELY(!(x))) {                                                           \
        FOCUS_LOG_ERROR(FOCUS_LOG_ROOT())   << "ASSERTION: " #x                          \
                                            << "\nbacktrace:\n"                          \
                                            << focus::BacktraceToString(100, 2, "    "); \
        assert(x);                                                                       \
    }

/// 断言宏封装
#define FOCUS_ASSERT2(x, w)                                                              \
    if(FOCUS_UNLIKELY(!(x))) {                                                           \
        FOCUS_LOG_ERROR(FOCUS_LOG_ROOT())   << "ASSERTION: " #x                          \
                                            << "\n" << w                                 \
                                            << "\nbacktrace:\n"                          \
                                            << focus::BacktraceToString(100, 2, "    "); \
        assert(x);                                                                       \
    }

#endif