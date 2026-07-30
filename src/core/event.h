//
// Created by 钟智强 on 2026/7/30.
//

#ifndef NEZHAGUARD_EVENT_H
#define NEZHAGUARD_EVENT_H

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "types.h"
#include "ipaddr.h"

namespace Nezha {
    namespace Core {
        struct FieldVal {
            enum class Kind : std::uint8_t { Int, Str } kind = Kind::Int;

            std::int64_t i = 0;
            std::string_view s{};

            static FieldVal num(std::int64_t v) {
                FieldVal f;
                f.kind = Kind::Int;
                f.i = v;
                return f;
            }

            static FieldVal str(std::string_view v) {
                FieldVal f;
                f.kind = Kind::Str;
                f.s = v;
                return f;
            }
        };


        class FieldMap {
        public:
            void put(FieldId id, FieldVal v) { kv_.emplace_back(id, v); }

            [[nodiscard]] const FieldVal *get(FieldId id) const noexcept {
                for (const auto &p: kv_) if (p.first == id) return &p.second;
                return nullptr;
            }

            [[nodiscard]] bool empty() const noexcept { return kv_.empty(); }
            [[nodiscard]] std::size_t size() const noexcept { return kv_.size(); }

            using const_iterator = std::vector<std::pair<FieldId, FieldVal> >::const_iterator;

            [[nodiscard]] const_iterator begin() const noexcept { return kv_.begin(); }
            [[nodiscard]] const_iterator end() const noexcept { return kv_.end(); }

        private:
            std::vector<std::pair<FieldId, FieldVal> > kv_;
        };

        // 三路来源(packet/log/honeypot)归一化后的唯一记录。
        // 这是纯数据载体、无不变量、引擎会读取每个字段 → 用 struct(公开成员)。
        // 与 ipaddr(有不变量→class) 的区别正是 struct/class 的判断标准。
        struct event {
            Nanos ts_ns = 0;
            EventSource source = EventSource::Packet;
            AppId app = 0;
            std::uint8_t proto = 0;
            Severity level = Severity::Info;
            Nezha::IPAddress::ipaddr src{};
            Nezha::IPAddress::ipaddr dst{};
            std::uint16_t sport = 0;
            std::uint16_t dport = 0;


            std::string_view msg{};
            FieldMap fields{};

            [[nodiscard]] std::string brief() const;
        };

        const char *to_cstr(EventSource) noexcept;

        const char *to_cstr(Severity) noexcept;
    }
}

#endif //NEZHAGUARD_EVENT_H
