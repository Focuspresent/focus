cmake_minimum_required(VERSION 3.10)
project(focus)

# 设置构建模式
set(CMAKE_BUILD_TYPE "Debug")

# 添加其余
include(cmake/utils.cmake)

# 设置bin/lib输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

# 添加头文件目录
include_directories(focus/)

# 添加库文件源码文件
set(LIB_SRC
    focus/log.cc)
add_library(focus ${LIB_SRC})

# 添加测试
self_add_executable(test_log tests/test_log.cc focus focus)