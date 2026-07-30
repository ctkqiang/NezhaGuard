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
        src/core/arena.cc
        src/core/arena.h
        src/core/capture.cc
        src/core/capture.h
        src/core/decoder.cc
        src/core/decoder.h
        src/core/log_watcher.cc
        src/core/log_watcher.h
        src/core/detector.cc
        src/core/detector.h
        src/core/honeypot.cc
        src/core/honeypot.h
        src/core/alert.cc
        src/core/alert.h
)

target_include_directories(NezhaGuard PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_library(PCAP_LIB pcap REQUIRED
    PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
)
find_path(PCAP_INCLUDE_DIR pcap.h REQUIRED
    PATHS /opt/homebrew/include /usr/local/include /usr/include
)
target_include_directories(NezhaGuard PRIVATE ${PCAP_INCLUDE_DIR})
target_link_libraries(NezhaGuard PRIVATE ${PCAP_LIB})
