//
// Created by 钟智强 on 2026/7/30.
//

#include "alert.h"
#include "../utilities/logger.h"
#include <algorithm>

namespace Nezha::Core {

    void AlertManager::submit(const Alert &a) {
        std::lock_guard<std::mutex> lock(mtx_);
        total_++;

        std::string ip(a.src_ip);
        Nanos cutoff = a.ts_ns - static_cast<Nanos>(dedup_sec_) * 1'000'000'000ULL;

        for (auto &dk: buffer_) {
            if (dk.type == a.type && dk.ip == ip && dk.first_ns >= cutoff) {
                dk.count++;
                dk.max_score = std::max(dk.max_score, a.score);
                if (a.level > dk.max_level) dk.max_level = a.level;
                if (!a.detail.empty()) dk.detail = std::string(a.detail);
                return;
            }
        }

        DedupKey dk{};
        dk.type = a.type;
        dk.ip = ip;
        dk.first_ns = a.ts_ns;
        dk.max_score = a.score;
        dk.max_level = a.level;
        dk.detail = a.detail.empty() ? std::string{} : std::string(a.detail);
        buffer_.push_back(std::move(dk));
    }

    void AlertManager::flush() {
        std::lock_guard<std::mutex> lock(mtx_);
        auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        Nanos cutoff = now - static_cast<Nanos>(dedup_sec_) * 1'000'000'000ULL;

        while (!buffer_.empty() && buffer_.front().first_ns < cutoff) {
            auto dk = buffer_.front();
            buffer_.pop_front();
            emit(dk);
        }
    }

    void AlertManager::emit(const DedupKey &dk) {
        using namespace Log;

        auto &logger = Logger::instance();
        auto lv = [](Severity s) -> Level {
            switch (s) {
                case Severity::Critical: return Level::Critical;
                case Severity::Error:    return Level::Error;
                case Severity::Warn:     return Level::Warn;
                default:                 return Level::Info;
            }
        }(dk.max_level);

        if (!logger.enabled(lv)) return;

        char score_buf[32];
        snprintf(score_buf, sizeof(score_buf), "%.0f", dk.max_score);

        std::string msg;
        msg.reserve(256);
        msg += "[";
        msg += attack_type_cstr(dk.type);
        msg += "] IP=";
        msg += dk.ip;
        msg += " count=";
        msg += std::to_string(dk.count);
        msg += " score=";
        msg += score_buf;
        if (!dk.detail.empty()) {
            msg += " 详情=";
            msg += dk.detail;
        }

        logger.log(lv, "安全告警: {}", msg);

        if (callback_) {
            Alert a{};
            a.type = dk.type;
            a.level = dk.max_level;
            a.score = dk.max_score;
            a.count = dk.count;
            callback_(a);
        }
    }

}
