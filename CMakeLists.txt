cmake_minimum_required(VERSION 3.20)
project(NezhaGuard)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(CLI_ONLY "Build CLI-only without Qt GUI" OFF)

add_compile_definitions($<$<CONFIG:Debug>:NEZHAGUARD_DEBUG>)

# ============================================================
# 核心源文件（GUI + CLI 共用）
# ============================================================
set(NEZHA_CORE_SOURCES
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
    src/service/notifier.cc
    src/service/notifier.h
    src/tools/types.h
    src/tools/exec.cc
    src/tools/exec.h
    src/tools/system_tool.cc
    src/tools/system_tool.h
    src/tools/grep.cc
    src/tools/grep.h
    src/tools/history.cc
    src/tools/history.h
    src/tools/ps.cc
    src/tools/ps.h
    src/tools/netstat.cc
    src/tools/netstat.h
    src/tools/lastlog.cc
    src/tools/lastlog.h
    src/tools/lastb.cc
    src/tools/lastb.h
    src/tools/faillog.cc
    src/tools/faillog.h
    src/tools/journalctl.cc
    src/tools/journalctl.h
    src/tools/tools.h
    src/contants.h
)

if(NOT CLI_ONLY)
    set(NEZHA_GUI_SOURCES
        main.cc
        src/views/monitor.cc
        src/views/monitor.h
        src/views/monitor.ui
        src/views/resources.qrc
        src/views/log_model.cc
        src/views/log_model.h
        src/views/gui_sink.cc
        src/views/gui_sink.h
        src/views/detail_panel.cc
        src/views/detail_panel.h
        src/views/radar_widget.cc
        src/views/radar_widget.h
        src/views/tools_page.cc
        src/views/tools_page.h
        src/views/spinner_widget.cc
        src/views/spinner_widget.h
        src/views/theme.h
    )
    set(NEZHA_ALL_SOURCES ${NEZHA_CORE_SOURCES} ${NEZHA_GUI_SOURCES})
else()
    add_compile_definitions(NEZHAGUARD_CLI_ONLY)
    set(NEZHA_ALL_SOURCES ${NEZHA_CORE_SOURCES} main.cc
            src/embedded/os/macos_notification.mm)
endif()

if(APPLE AND NOT CLI_ONLY)
    set(NEZHA_ALL_SOURCES ${NEZHA_ALL_SOURCES}
        src/embedded/os/macos_notification.mm
        src/embedded/os/macos_notification.h
    )
    add_executable(NezhaGuard MACOSX_BUNDLE
        ${NEZHA_ALL_SOURCES}
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
    add_executable(NezhaGuard ${NEZHA_ALL_SOURCES})
endif()

target_include_directories(NezhaGuard PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT CLI_ONLY)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)
endif()

find_library(PCAP_LIB pcap REQUIRED
    PATHS /opt/homebrew/lib /usr/local/lib /usr/lib /usr/lib/aarch64-linux-gnu /usr/lib/x86_64-linux-gnu
)
find_path(PCAP_INCLUDE_DIR pcap.h REQUIRED
    PATHS /opt/homebrew/include /usr/local/include /usr/include
)
target_include_directories(NezhaGuard PRIVATE ${PCAP_INCLUDE_DIR})

find_library(SQLITE3_LIB sqlite3 REQUIRED
    PATHS /opt/homebrew/lib /usr/local/lib /usr/lib /usr/lib/aarch64-linux-gnu /usr/lib/x86_64-linux-gnu
)
find_path(SQLITE3_INCLUDE_DIR sqlite3.h REQUIRED
    PATHS /opt/homebrew/opt/sqlite/include /opt/homebrew/Cellar/sqlite/3.53.3/include
          /opt/homebrew/include /usr/local/include /usr/include
)
target_include_directories(NezhaGuard PRIVATE ${SQLITE3_INCLUDE_DIR})

if(NOT CLI_ONLY)
    if(APPLE)
        find_library(USERNOTIFICATIONS_LIB UserNotifications REQUIRED)
        target_link_libraries(NezhaGuard PRIVATE ${PCAP_LIB} Qt6::Widgets Qt6::Concurrent ${SQLITE3_LIB} ${USERNOTIFICATIONS_LIB})
    else()
        target_link_libraries(NezhaGuard PRIVATE ${PCAP_LIB} Qt6::Widgets Qt6::Concurrent ${SQLITE3_LIB})
    endif()
else()
    target_link_libraries(NezhaGuard PRIVATE ${PCAP_LIB} ${SQLITE3_LIB})
endif()

# ============================================================
# 安装规则
# ============================================================
include(GNUInstallDirs)

if(APPLE AND NOT CLI_ONLY)
    set(INSTALL_BIN_DIR .)
    set(INSTALL_DATA_DIR "Contents/Resources")
    install(TARGETS NezhaGuard BUNDLE DESTINATION .)
    install(FILES rules/default.yaml DESTINATION "Contents/Resources/rules")
elseif(NOT CLI_ONLY)
    install(TARGETS NezhaGuard RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES rules/default.yaml DESTINATION /etc/nezhaguard/rules)
    install(DIRECTORY config/ DESTINATION /etc/nezhaguard/config
            FILES_MATCHING PATTERN "*.conf"
            PATTERN "notifier" EXCLUDE)
    install(DIRECTORY config/notifier/ DESTINATION /etc/nezhaguard/config/notifier
            FILES_MATCHING PATTERN "*.conf")
else()
    install(TARGETS NezhaGuard RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES rules/default.yaml DESTINATION /etc/nezhaguard/rules)
    install(DIRECTORY config/ DESTINATION /etc/nezhaguard/config
            FILES_MATCHING PATTERN "*.conf"
            PATTERN "notifier" EXCLUDE)
    install(DIRECTORY config/notifier/ DESTINATION /etc/nezhaguard/config/notifier
            FILES_MATCHING PATTERN "*.conf")
endif()

# ============================================================
# CPack
# ============================================================
if(NOT CLI_ONLY)
    set(CPACK_PACKAGE_NAME "nezhaguard")
    set(CPACK_PACKAGE_VERSION "0.0.1")
    set(CPACK_PACKAGE_VENDOR "钟智强")
    set(CPACK_PACKAGE_CONTACT "johnmelodymel@qq.com")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "哪吒网络安全 SIEM — 蓝队主动防御平台")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ctkqiang/NezhaGuard")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

    if(NOT APPLE)
        set(CPACK_GENERATOR "DEB")
        set(CPACK_DEBIAN_PACKAGE_NAME "nezhaguard")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "钟智强 <johnmelodymel@qq.com>")
        set(CPACK_DEBIAN_PACKAGE_SECTION "net")
        set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
        set(CPACK_DEBIAN_PACKAGE_DEPENDS
            "libqt6widgets6t64 (>= 6.5.0), libpcap0.8t64 (>= 1.10.0), libsqlite3-0 (>= 3.40.0)")
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
        set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
            "${CMAKE_SOURCE_DIR}/scripts/deb-postinst;${CMAKE_SOURCE_DIR}/scripts/deb-postrm")
    endif()

    include(CPack)
endif()
