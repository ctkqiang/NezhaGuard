//
// Created by 钟智强 on 2026/7/30.
//

#include "event.h"
#include <sstream>

namespace Nezha {
    namespace Core {
        const char *to_cstr(EventSource s) noexcept {
            switch (s) {
                case EventSource::Packet: return "数据包";
                case EventSource::Log: return "日志";
                case EventSource::Honeypot: return "蜜罐";
            }
            return "?";
        }

        const char *to_cstr(Severity s) noexcept {
            switch (s) {
                case Severity::Trace: return "跟踪";
                case Severity::Debug: return "调试";
                case Severity::Info: return "信息";
                case Severity::Warn: return "警告";
                case Severity::Error: return "错误";
                case Severity::Critical: return "严重";
            }
            return "?";
        }

        std::string event::brief() const {
            std::ostringstream os;
            os << '[' << to_cstr(source) << "] "
                    << src.to_string() << ':' << sport << " -> "
                    << dst.to_string() << ':' << dport
                    << " 协议=" << static_cast<unsigned>(proto)
                    << " 字段=" << fields.size();
            if (!msg.empty()) os << " 消息=\"" << msg << '"';
            return os.str();
        }
    }
}
