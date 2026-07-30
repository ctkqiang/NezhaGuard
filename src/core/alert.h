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

    enum class AlertSink { Log, File, Webhook };

    class AlertManager {
    public:
        AlertManager() = default;

        void submit(const Alert &a);
        void set_callback(std::function<void(const Alert &)> cb) { callback_ = std::move(cb); }
        void flush();

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
