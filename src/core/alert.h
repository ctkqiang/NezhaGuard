//
// Created by 钟智强 on 2026/7/30.
//

#ifndef NEZHAGUARD_ALERT_H
#define NEZHAGUARD_ALERT_H

#include "detector.h"
#include <deque>
#include <functional>
#include <mutex>
#include <string>

namespace Nezha::Core {

    // 告警输出目标
    enum class AlertSink { Log, File, Webhook };

    // 告警管理器：去重、聚合、分级路由
    class AlertManager {
    public:
        AlertManager() = default;

        // 提交一条告警，自动去重，达到阈值时触发输出
        void submit(const Alert &a);

        // 设置告警回调（用于 Webhook 等自定义输出）
        void set_callback(std::function<void(const Alert &)> cb) { callback_ = std::move(cb); }

        // 手动刷新去重缓冲
        void flush();

        // 去重窗口大小（同类型同 IP 在此窗口内合并，秒）
        void set_dedup_window(int seconds) { dedup_sec_ = seconds; }

        [[nodiscard]] std::size_t total_alerts() const noexcept { return total_; }

    private:
        struct DedupKey {
            AttackType type;
            std::string ip;
            Nanos first_ns;
            std::uint32_t count = 1;
            double max_score = 0.0;
            Severity max_level = Severity::Info;
            std::string detail;
        };

        void emit(const DedupKey &dk);

        std::mutex mtx_;
        std::deque<DedupKey> buffer_;
        int dedup_sec_ = 60;
        std::size_t total_ = 0;

        std::function<void(const Alert &)> callback_;
    };
}

#endif //NEZHAGUARD_ALERT_H
