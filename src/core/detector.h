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

    // 攻击类别
    enum class AttackType : std::uint8_t {
        SQLi,          // SQL 注入
        XSS,           // 跨站脚本
        PathTraversal,  // 路径穿越
        CmdInjection,  // 命令注入
        FileInclusion, // 文件包含 (LFI/RFI)
        Scanner,       // 扫描器/探测
        BruteForce,    // 暴力破解
        PortScan,      // 端口扫描
        Webshell,      // Webshell 上传/访问
        Log4j,         // Log4j 漏洞利用
        BotActivity,   // 恶意爬虫/机器人
        RateAnomaly,   // 速率异常 (DDoS)
        UnknownMal,    // 未分类恶意行为
    };

    const char *attack_type_cstr(AttackType t) noexcept;

    // 检测结果
    struct Alert {
        AttackType type = AttackType::UnknownMal;
        Severity level = Severity::Info;
        Nanos ts_ns = 0;
        std::string_view evidence{};   // 匹配证据
        std::string_view src_ip{};     // 来源 IP 字符串
        std::string_view detail{};     // 补充说明
        std::uint32_t count = 1;       // 同类事件计数
        double score = 0.0;            // 威胁评分 0-100
    };

    using AlertCallback = std::function<void(const Alert &)>;

    // 攻击检测引擎：签名匹配 + 速率追踪 + 威胁评分
    class AttackDetector {
    public:
        AttackDetector() = default;

        // 输入一个 event，分析并回调发现的告警
        void analyze(const event &e, Arena &arena, AlertCallback cb);

        // 清理过期速率数据
        void expire_counters(Nanos now_ns);

    private:
        // --- 签名匹配 ----
        struct SigRule {
            AttackType type;
            Severity level;
            double score;
            const char *pattern;
            const char *desc;
        };

        static const SigRule *match_signature(std::string_view msg, std::string_view ua);

        // --- 速率追踪 ---
        struct RateEntry {
            std::uint32_t count = 0;
            Nanos last_ns = 0;
        };
        using IpPortKey = std::uint64_t;
        std::unordered_map<IpPortKey, RateEntry> rates_;
        Nanos last_expire_ = 0;

        static IpPortKey make_key(const event &e) noexcept;
    };
}

#endif //NEZHAGUARD_DETECTOR_H
