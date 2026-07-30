//
// Created by 钟智强 on 2026/7/30.
//

#ifndef NEZHAGUARD_TYPES_H
#define NEZHAGUARD_TYPES_H

#include <cstdint>

// 跨子系统的基础基元(prelude)。放在顶层 Nezha，避免 Nezha::Core::Nanos 这种冗长。
namespace Nezha {
    using Nanos = std::uint64_t; // 时间戳/时长(纳秒)
    using AppId = std::uint16_t; // 应用名 intern 后的 id
    using FieldId = std::uint32_t; // 字段名 intern 后的 id

    // L4 协议号(取自 IP 头 protocol 字段)
    enum : std::uint8_t { PROTO_ICMP = 1, PROTO_TCP = 6, PROTO_UDP = 17 };

    // 安全事件严重级别(与日志 Level 概念相近，但用于事件本身)
    enum class Severity : std::uint8_t { Trace, Debug, Info, Warn, Error, Critical };

    // 事件来源
    enum class EventSource : std::uint8_t { Packet, Log, Honeypot };
}

#endif //NEZHAGUARD_TYPES_H
