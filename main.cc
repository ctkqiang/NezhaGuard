#include <iostream>

#include "src/core/ipaddr.h"
#include "src/utilities/logger.h"

int main() {
    // 初始化日志系统（文件路径 logs/nezha.log，级别 Info）
    Nezha::Log::init_default("logs/nezha.log", Nezha::Log::Level::Info);

    Nezha::IPAddress::ipaddr addr;
    if (Nezha::IPAddress::ipaddr::parse("127.0.0.1", addr)) {
        // 使用正确的宏名称（NZ_INFO，而非 LNZ_INFO）
        NZ_INFO("解析的地址: {} (环回={})",
                addr.to_string(), addr.is_loopback());
    }

    Nezha::Log::Logger::instance().flush();
    return 0;
}