//
// Created by 钟智强 on 2026/8/2.
//

#pragma once

#ifndef NEZHAGUARD_NOTIFIER_H
#define NEZHAGUARD_NOTIFIER_H

#include "../core/types.h"
#include "../core/detector.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Nezha::Service {

enum class NotifyChannel : std::uint8_t {
    Slack,
    Discord,
    WeChat,
    DingTalk,
    Email,
    LocalGui,
    Feishu
};

const char *channel_name(NotifyChannel ch) noexcept;

struct ChannelConfig {
    NotifyChannel channel;
    std::string webhook_url;
    bool enabled = false;
};

struct TriggerRule {
    std::vector<std::string> keywords;
    std::vector<NotifyChannel> channels;
    Severity min_level = Severity::Warn;
};

class Notifier {
public:
    static Notifier &instance() noexcept;

    Notifier(const Notifier &) = delete;
    Notifier &operator=(const Notifier &) = delete;

    void configure_channel(const ChannelConfig &cfg);
    void add_trigger(const TriggerRule &rule);
    int load_rules_from_file(const std::string &path);

    void set_gui_callback(std::function<void(const std::string &, const std::string &)> cb);

    void on_alert(const Core::Alert &alert);

private:
    Notifier() = default;

    void dispatch(const TriggerRule &rule, const std::string &title, const std::string &body);
    void send_slack(const std::string &webhook, const std::string &msg);
    void send_discord(const std::string &webhook, const std::string &msg);
    void send_dingtalk(const std::string &webhook, const std::string &msg);
    void send_feishu(const std::string &webhook, const std::string &msg);
    void send_wechat(const std::string &webhook, const std::string &msg);
    void send_email(const std::string &to, const std::string &subject, const std::string &body);
    void send_local_gui(const std::string &title, const std::string &body);

    static bool keyword_match(const std::string &text, const std::vector<std::string> &keywords);
    static std::string http_post(const std::string &url, const std::string &json);

    std::mutex mtx_;
    std::unordered_map<NotifyChannel, ChannelConfig> channels_;
    std::vector<TriggerRule> rules_;
    std::function<void(const std::string &, const std::string &)> gui_callback_;
};

} // namespace Nezha::Service

#endif //NEZHAGUARD_NOTIFIER_H
