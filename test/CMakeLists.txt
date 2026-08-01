cmake_minimum_required(VERSION 3.22)
project(NezhaGuardTests)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_AUTOMOC ON)

enable_testing()

set(TEST_SOURCES
    test_main.cc
    test_ipaddr.cc
    test_arena.cc
    test_types.cc
    test_log_model.cc
    test_theme.cc
)

add_executable(NezhaGuardTests ${TEST_SOURCES})

target_include_directories(NezhaGuardTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

find_package(Qt6 REQUIRED COMPONENTS Test Widgets Concurrent)

find_library(SQLITE3_LIB sqlite3 REQUIRED
    PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
)
find_path(SQLITE3_INCLUDE_DIR sqlite3.h REQUIRED
    PATHS /opt/homebrew/opt/sqlite/include /opt/homebrew/include /usr/local/include /usr/include
)
target_include_directories(NezhaGuardTests PRIVATE ${SQLITE3_INCLUDE_DIR})

# Core sources needed (excluding main.cc)
set(NEZHA_CORE_TEST
    ../src/core/ipaddr.cc
    ../src/core/arena.cc
    ../src/core/event.cc
    ../src/core/honeypot.cc
    ../src/views/log_model.cc
    ../src/views/gui_sink.cc
    ../src/views/detail_panel.cc
    ../src/service/database_helper.cc
    ../src/core/tor_checker.cc
    ../src/core/geo_ip.cc
    ../src/core/active_response.cc
    ../src/core/capture.cc
    ../src/core/decoder.cc
    ../src/core/log_watcher.cc
    ../src/core/detector.cc
    ../src/core/alert.cc
    ../src/core/net_util.cc
    ../src/utilities/logger.cc
)

target_sources(NezhaGuardTests PRIVATE ${NEZHA_CORE_TEST})

find_library(PCAP_LIB pcap REQUIRED
    PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
)
find_path(PCAP_INCLUDE_DIR pcap.h REQUIRED
    PATHS /opt/homebrew/include /usr/local/include /usr/include
)
target_include_directories(NezhaGuardTests PRIVATE ${PCAP_INCLUDE_DIR})

target_link_libraries(NezhaGuardTests PRIVATE
    Qt6::Test Qt6::Widgets Qt6::Concurrent ${PCAP_LIB} ${SQLITE3_LIB}
)

add_test(NAME NezhaGuardTests COMMAND NezhaGuardTests)
