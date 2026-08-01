//
// Created by 钟智强 on 2026/7/30.
//

#ifndef NEZHAGUARD_DETECTOR_H
#define NEZHAGUARD_DETECTOR_H

#include "types.h"
#include "event.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

namespace Nezha::Core {
    class Arena;

    enum class AttackType : std::uint8_t {
        SQLi,
        XSS,
        PathTraversal,
        CmdInjection,
        FileInclusion,
        Scanner,
        BruteForce,
        PortScan,
        Webshell,
        Log4j,
        BotActivity,
        RateAnomaly,
        UnknownMal,
    };

    const char *attack_type_cstr(AttackType t) noexcept;

    struct SigRule {
        AttackType type;
        Severity level;
        double score;
        std::string pattern;
        std::string desc;
    };

    struct Alert {
        AttackType type = AttackType::UnknownMal;
        Severity level = Severity::Info;
        Nanos ts_ns = 0;
        std::string_view evidence{};
        std::string_view src_ip{};
        std::string_view detail{};
        std::uint32_t count = 1;
        double score = 0.0;
    };

    using AlertCallback = std::function<void(const Alert &)>;

    class AttackDetector {
    public:
        AttackDetector();

        void analyze(const event &e, Arena &arena, AlertCallback cb);
        void expire_counters(Nanos now_ns);

        // 运行时规则加载：从文件加载，成功返回 true，失败保留旧规则并告警
        bool load_rules(const std::string &path);

        // SIGHUP 热重载：调用 load_rules
        void reload_rules(const std::string &path);

        [[nodiscard]] std::size_t rule_count() const noexcept { return rules_.size(); }

    private:
        struct RateEntry {
            std::uint32_t count = 0;
            Nanos last_ns = 0;
        };
        using IpPortKey = std::uint64_t;
        std::unordered_map<IpPortKey, RateEntry> rates_;
        std::vector<SigRule> rules_;
        Nanos last_expire_ = 0;

        static IpPortKey make_key(const event &e) noexcept;
    };
}

#endif //NEZHAGUARD_DETECTOR_H
