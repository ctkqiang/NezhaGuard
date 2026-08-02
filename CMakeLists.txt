cmake_minimum_required(VERSION 4.3)
project(NezhaGuard)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_compile_definitions($<$<CONFIG:Debug>:NEZHAGUARD_DEBUG>)

set(NEZHA_CORE_SOURCES
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
    src/core/rule_loader.cc
    src/core/rule_loader.h
    src/core/honeypot.cc
    src/core/honeypot.h
    src/core/webshell_scanner.cc
    src/core/webshell_scanner.h
    src/core/alert.cc
    src/core/alert.h
    src/model/request.h
    src/model/severity.h
    src/core/net_util.cc
    src/core/net_util.h
    src/core/tor_checker.cc
    src/core/tor_checker.h
    src/core/geo_ip.cc
    src/core/geo_ip.h
    src/core/ipcn.cc
    src/core/ipcn.h
    src/core/active_response.cc
    src/core/active_response.h
    src/core/application_monitor.cc
    src/core/application_monitor.h
    src/service/database_helper.cc
    src/service/database_helper.h
    src/views/monitor.cc
    src/views/monitor.h
    src/views/monitor.ui
    src/views/log_model.cc
    src/views/log_model.h
    src/views/gui_sink.cc
    src/views/gui_sink.h
    src/views/detail_panel.cc
    src/views/detail_panel.h
    src/views/radar_widget.cc
    src/views/radar_widget.h
    src/views/theme.h
    src/contants.h
)

if(APPLE)
    add_executable(NezhaGuard MACOSX_BUNDLE
        ${NEZHA_CORE_SOURCES}
        src/views/app_icon.svg
        src/views/app_icon.icns
        src/Info.plist
    )
    set_target_properties(NezhaGuard PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "哪吒网络安全 SIEM"
        MACOSX_BUNDLE_BUNDLE_VERSION "0.0.1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.0.1"
        MACOSX_BUNDLE_ICON_FILE "app_icon.icns"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist"
    )
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/src/views/app_icon.icns" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    set(CMAKE_PREFIX_PATH
        /opt/homebrew/opt/qt
        /usr/local/opt/qt
        ${CMAKE_PREFIX_PATH}
    )
else()
    add_executable(NezhaGuard ${NEZHA_CORE_SOURCES})
endif()

target_include_directories(NezhaGuard PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

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
target_link_libraries(NezhaGuard PRIVATE ${PCAP_LIB} Qt6::Widgets Qt6::Concurrent ${SQLITE3_LIB})
