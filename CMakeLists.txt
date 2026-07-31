cmake_minimum_required(VERSION 4.3)
project(NezhaGuard)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

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
        src/model/request.h
        src/model/severity.h
        src/core/net_util.cc
        src/core/net_util.h
        src/service/database_helper.cc
        src/service/database_helper.h
        src/views/monitor.cc
        src/views/monitor.h
        src/views/monitor.ui
        src/views/log_model.cc
        src/views/log_model.h
        src/views/gui_sink.cc
        src/views/gui_sink.h
        src/contants.h
)

target_include_directories(NezhaGuard PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Qt6 Homebrew 路径 (Apple Silicon / Intel)
set(CMAKE_PREFIX_PATH
    /opt/homebrew/opt/qt
    /usr/local/opt/qt
    ${CMAKE_PREFIX_PATH}
)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

find_library(PCAP_LIB pcap REQUIRED
    PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
)
find_path(PCAP_INCLUDE_DIR pcap.h REQUIRED
    PATHS /opt/homebrew/include /usr/local/include /usr/include
)
target_include_directories(NezhaGuard PRIVATE ${PCAP_INCLUDE_DIR})

find_library(SQLITE3_LIB sqlite3 REQUIRED
    PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
)
find_path(SQLITE3_INCLUDE_DIR sqlite3.h REQUIRED
    PATHS /opt/homebrew/opt/sqlite/include /opt/homebrew/Cellar/sqlite/3.53.3/include
          /opt/homebrew/include /usr/local/include /usr/include
)
target_include_directories(NezhaGuard PRIVATE ${SQLITE3_INCLUDE_DIR})
target_link_libraries(NezhaGuard PRIVATE ${PCAP_LIB} Qt6::Widgets ${SQLITE3_LIB})
