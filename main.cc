#include <iostream>
#include "src/utilities/logger.h"

int main() {
    Nezha::Log::init_default("logs/nezha.log", Nezha::Log::Level::Info);

    NZ_INFO("哪吒系统已启动.....");

    Nezha::Log::Logger::instance().flush();
    return 0;
}