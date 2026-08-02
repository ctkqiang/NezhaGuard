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
    Feishu,
    Telegram
};

const char *channel_name(NotifyChannel ch) noexcept;
NotifyChannel channel_from_name(const std::string &name);

struct ChannelConfig {
    NotifyChannel channel;
    std::string webhook_url;
    std::string chat_id;
    std::vector<std::string> keywords;
    Severity min_level = Severity::Warn;
    bool enabled = false;
};

class Notifier {
public:
    static Notifier &instance() noexcept;

    Notifier(const Notifier &) = delete;
    Notifier &operator=(const Notifier &) = delete;

    void configure_channel(const ChannelConfig &cfg);
    int load_config_dir(const std::string &dir_path);

    void set_gui_callback(std::function<void(const std::string &, const std::string &)> cb);

    void on_alert(const Core::Alert &alert);

private:
    Notifier() = default;

    void dispatch(const ChannelConfig &cfg, const std::string &title, const std::string &body);
    void send_slack(const std::string &webhook, const std::string &msg);
    void send_discord(const std::string &webhook, const std::string &msg);
    void send_dingtalk(const std::string &webhook, const std::string &msg);
    void send_feishu(const std::string &webhook, const std::string &msg);
    void send_wechat(const std::string &webhook, const std::string &msg);
    void send_email(const std::string &to, const std::string &subject, const std::string &body);
    void send_local_gui(const std::string &title, const std::string &body);
    void send_telegram(const std::string &token, const std::string &chat, const std::string &msg);

    static bool keyword_match(const std::string &text, const std::vector<std::string> &keywords);
    static std::string http_post(const std::string &url, const std::string &json);

    std::mutex mtx_;
    std::unordered_map<NotifyChannel, ChannelConfig> channels_;
    std::function<void(const std::string &, const std::string &)> gui_callback_;
};

} // namespace Nezha::Service

#endif //NEZHAGUARD_NOTIFIER_H
