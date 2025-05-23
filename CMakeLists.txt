cmake_minimum_required(VERSION 3.10)
project(focus)

# 设置构建模式
set(CMAKE_BUILD_TYPE "Debug")

# 添加其余
include(cmake/utils.cmake)

# 设置bin/lib输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

# 添加头文件目录
include_directories(focus/)

# 添加库文件源码文件
set(LIB_SRC
    focus/log.cc
    focus/mutex.cc
    focus/config.cc
    focus/util.cc
    focus/thread.cc
    focus/fiber.cc
    focus/scheduler.cc
    focus/timer.cc
    focus/iomanager.cc
    focus/env.cc
    focus/fdmanager.cc
    focus/hook.cc)
add_library(focus ${LIB_SRC})
target_link_libraries(focus PUBLIC pthread yaml-cpp dl)
target_compile_options(focus PUBLIC -rdynamic)

# 添加测试
add_executable(test_demo tests/test_demo)
add_executable(test_ucontext tests/test_ucontext)
add_executable(test_epoll tests/test_epoll)

self_add_executable(test_log tests/test_log.cc focus focus)
self_add_executable(test_config tests/test_config.cc focus focus)
self_add_executable(test_thread tests/test_thread.cc focus focus)
self_add_executable(test_yaml tests/test_yaml.cc focus focus)
self_add_executable(test_macro tests/test_macro.cc focus focus)
self_add_executable(test_fiber tests/test_fiber.cc focus focus)
self_add_executable(test_scheduler tests/test_scheduler.cc focus focus)
self_add_executable(test_env tests/test_env.cc focus focus)
self_add_executable(test_iomanager tests/test_iomanager.cc focus focus)