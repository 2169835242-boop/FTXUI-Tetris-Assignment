cmake_minimum_required(VERSION 3.14)
project(ftxui_tetris LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 【核心修复】：为 Windows 平台下的 MinGW 强制开启 UNICODE 宏定义，解决 92% 的报错
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
endif()

# 包含头文件目录
include_directories(include)
include_directories(src)

# 精准抓取 FTXUI 的三大核心组件目录
file(GLOB_RECURSE FTXUI_CORE_DOM "src/ftxui/dom/*.cpp")
file(GLOB_RECURSE FTXUI_CORE_SCREEN "src/ftxui/screen/*.cpp")
file(GLOB_RECURSE FTXUI_CORE_COMP "src/ftxui/component/*.cpp")

# 将真正需要的核心代码组合起来
set(FTXUI_SOURCES ${FTXUI_CORE_DOM} ${FTXUI_CORE_SCREEN} ${FTXUI_CORE_COMP})

# 严格过滤：凡是路径里带有 test、fuzzer 或 example 的文件，一律剔除
set(FILTERED_SOURCES "")
foreach(source_file ${FTXUI_SOURCES})
    if(NOT source_file MATCHES "test" AND NOT source_file MATCHES "fuzzer" AND NOT source_file MATCHES "example")
        list(APPEND FILTERED_SOURCES ${source_file})
    endif()
endforeach()

# 编译大作业主程序
add_executable(ftxui_tetris main.cpp ${FILTERED_SOURCES})