//
// Created by 钟智强 on 2026/7/30.
//

#ifndef NEZHAGUARD_DECODER_H
#define NEZHAGUARD_DECODER_H

#include "types.h"
#include "event.h"

namespace Nezha::Core {
    class Arena;

    // 协议解码：从原始链路层帧中提取 L3/L4/L7 信息，填入 Core::event
    class ProtocolDecoder {
    public:
        // raw=以太网帧, len=帧长, ts=时间戳, arena=字符串驻留, out=输出事件
        // 返回 true 表示成功解析出 IP 层
        static bool decode(const std::uint8_t *raw,
                           std::size_t len,
                           const timeval &ts,
                           Arena &arena,
                           event &out);

    private:
        // 从 TCP 负载中解析 HTTP 请求行和 Host 头
        static void parse_http(const std::uint8_t *payload,
                               std::size_t len,
                               Arena &arena,
                               event &e);
    };
}

#endif //NEZHAGUARD_DECODER_H
