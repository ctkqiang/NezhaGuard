cmake_minimum_required(VERSION 4.3)
project(NezhaGuard)

set(CMAKE_CXX_STANDARD 26)

add_executable(NezhaGuard
        main.cc
        src/utilities/logger.cc
        src/utilities/logger.h
        src/core/ipaddr.cc
        src/core/ipaddr.h
        src/core/event.cc
        src/core/event.h
        src/core/types.h
)