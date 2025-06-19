# 指定 CMake 的最小版本要求，建议使用 3.10 及以上，兼容性好
cmake_minimum_required(VERSION 3.10)

# 设置项目名称为 ReactorServer
project(ReactorServer)

# 设置 C++ 标准为 C++11，可以根据需要改为 14 / 17 / 20
set(CMAKE_CXX_STANDARD 11)

# 定义一个可执行文件，名为 tcpepoll，包含源码文件 tcpepoll.cpp
add_executable(tcpepoll tcpepoll.cpp)
