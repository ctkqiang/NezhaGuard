//
// Created by 钟智强 on 2026/7/30.
//

#ifndef NEZHAGUARD_DECODER_H
#define NEZHAGUARD_DECODER_H

#include "types.h"
#include "event.h"

namespace Nezha::Core {
    class Arena;

    class ProtocolDecoder {
    public:
        static bool decode(const std::uint8_t *raw,
                           std::size_t len,
                           const timeval &ts,
                           Arena &arena,
                           event &out);

    private:
        static void parse_http(const std::uint8_t *payload,
                               std::size_t len,
                               Arena &arena,
                               event &e);
    };
}

#endif //NEZHAGUARD_DECODER_H
