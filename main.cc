#include <iostream>
#include <cstdarg>

#include "src/core/arena.h"
#include "src/core/event.h"
#include "src/core/ipaddr.h"
#include "src/utilities/logger.h"

using namespace Nezha;

namespace {
    void log_info(const char *file, int line, const char *fmt, ...) {
        auto &logger = Log::Logger::instance();
        if (!logger.enabled(Log::Level::Info)) return;
        va_list ap;
        va_start(ap, fmt);
        logger.vlogf(Log::Level::Info, file, line, fmt, ap);
        va_end(ap);
    }
}

int main() {
    Log::init_default("logs/nezha.log", Log::Level::Trace);
    Core::Arena arena(64 * 1024);

    IPAddress::ipaddr src, dst;
    IPAddress::ipaddr::parse("203.0.113.9", src);
    IPAddress::ipaddr::parse("10.0.0.1", dst);

    Core::event e;

    e.source = EventSource::Log;
    e.proto = PROTO_TCP;
    e.src = src;
    e.dst = dst;
    e.sport = 40001;
    e.dport = 2222;
    e.msg = arena.intern("POST /wp-login.php HTTP/1.1");
    e.fields.put(100, Core::FieldVal::str(arena.intern("POST")));

    log_info(__FILE__, __LINE__, "演示事件: %s", e.brief().c_str());

    log_info(__FILE__, __LINE__,
             "源 %s 私网=%s 环回=%s",
             src.to_string().c_str(),
             src.is_private() ? "true" : "false",
             src.is_loopback() ? "true" : "false"
    );

    Log::Logger::instance().flush();

    return 0;
}
