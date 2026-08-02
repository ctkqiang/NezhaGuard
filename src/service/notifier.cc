//
// Created by 钟智强 on 2026/8/2.
//

#include "notifier.h"
#include "../core/detector.h"
#include "../utilities/logger.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <fstream>
#include <memory>
#include <sstream>
#include <thread>

namespace Nezha::Service {

const char *channel_name(NotifyChannel ch) noexcept {
    switch (ch) {
        case NotifyChannel::Slack:    return "Slack";
        case NotifyChannel::Discord:  return "Discord";
        case NotifyChannel::WeChat:   return "WeChat";
        case NotifyChannel::DingTalk: return "DingTalk";
        case NotifyChannel::Email:    return "Email";
        case NotifyChannel::LocalGui: return "LocalGui";
        case NotifyChannel::Feishu:   return "Feishu";
        default:                      return "???";
    }
}

// -- 单例 ----------------------------------------------------------------
Notifier &Notifier::instance() noexcept {
    static Notifier inst;
    return inst;
}

// -- 配置 ----------------------------------------------------------------
void Notifier::configure_channel(const ChannelConfig &cfg) {
    std::lock_guard<std::mutex> lock(mtx_);
    channels_[cfg.channel] = cfg;
    if (cfg.enabled)
        NZ_INFO("[通知] 渠道已配置: {} -> {}", channel_name(cfg.channel), cfg.webhook_url);
}

void Notifier::add_trigger(const TriggerRule &rule) {
    std::lock_guard<std::mutex> lock(mtx_);
    rules_.push_back(rule);
}

void Notifier::set_gui_callback(std::function<void(const std::string &, const std::string &)> cb) {
    std::lock_guard<std::mutex> lock(mtx_);
    gui_callback_ = std::move(cb);
}

// -- 配置加载 ----------------------------------------------------------------
int Notifier::load_rules_from_file(const std::string &path) {
    std::ifstream f(path);
    if (!f.is_open()) return -1;

    int loaded = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;

        // channel=webhook_url
        if (line.starts_with("slack=")) {
            configure_channel({NotifyChannel::Slack, line.substr(6), true});
            ++loaded;
        } else if (line.starts_with("discord=")) {
            configure_channel({NotifyChannel::Discord, line.substr(8), true});
            ++loaded;
        } else if (line.starts_with("dingtalk=")) {
            configure_channel({NotifyChannel::DingTalk, line.substr(9), true});
            ++loaded;
        } else if (line.starts_with("feishu=")) {
            configure_channel({NotifyChannel::Feishu, line.substr(7), true});
            ++loaded;
        } else if (line.starts_with("wechat=")) {
            configure_channel({NotifyChannel::WeChat, line.substr(7), true});
            ++loaded;
        } else if (line.starts_with("email=")) {
            configure_channel({NotifyChannel::Email, line.substr(6), true});
            ++loaded;
        } else if (line.starts_with("local=")) {
            configure_channel({NotifyChannel::LocalGui, {}, line.substr(6) == "true" || line.substr(6) == "1"});
            ++loaded;
        } else if (line.starts_with("rule:")) {
            // rule: keyword1,keyword2,... -> slack,discord,local
            auto arrow = line.find("->");
            if (arrow == std::string::npos) continue;

            std::string kw_part = line.substr(5, arrow - 5);
            std::string ch_part = line.substr(arrow + 2);

            TriggerRule rule;
            rule.min_level = Severity::Warn;

            // 解析关键词
            std::istringstream kw_ss(kw_part);
            std::string kw;
            while (std::getline(kw_ss, kw, ',')) {
                // trim
                auto s = kw.find_first_not_of(" \t");
                auto e = kw.find_last_not_of(" \t");
                if (s != std::string::npos)
                    rule.keywords.push_back(kw.substr(s, e - s + 1));
            }

            // 解析渠道
            std::istringstream ch_ss(ch_part);
            std::string ch;
            while (std::getline(ch_ss, ch, ',')) {
                auto s = ch.find_first_not_of(" \t");
                auto e = ch.find_last_not_of(" \t");
                if (s == std::string::npos) continue;
                std::string cn = ch.substr(s, e - s + 1);

                if (cn == "slack") rule.channels.push_back(NotifyChannel::Slack);
                else if (cn == "discord") rule.channels.push_back(NotifyChannel::Discord);
                else if (cn == "dingtalk") rule.channels.push_back(NotifyChannel::DingTalk);
                else if (cn == "feishu") rule.channels.push_back(NotifyChannel::Feishu);
                else if (cn == "wechat") rule.channels.push_back(NotifyChannel::WeChat);
                else if (cn == "email") rule.channels.push_back(NotifyChannel::Email);
                else if (cn == "local") rule.channels.push_back(NotifyChannel::LocalGui);
            }

            if (!rule.keywords.empty() && !rule.channels.empty()) {
                add_trigger(rule);
                ++loaded;
            }
        }
    }
    NZ_INFO("[通知] 已加载 {} 条配置", loaded);
    return loaded;
}

// -- 关键词匹配 ----------------------------------------------------------------
bool Notifier::keyword_match(const std::string &text, const std::vector<std::string> &keywords) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto &kw: keywords) {
        std::string kl = kw;
        std::transform(kl.begin(), kl.end(), kl.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower.find(kl) != std::string::npos) return true;
    }
    return false;
}

// -- 告警入口 ----------------------------------------------------------------
void Notifier::on_alert(const Core::Alert &alert) {
    std::string type_str = Core::attack_type_cstr(alert.type);
    std::string ip(alert.src_ip);
    std::string evidence(alert.evidence);
    std::string detail(alert.detail);

    std::string combined = type_str + " " + ip + " " + evidence + " " + detail;

    std::string title = std::format("[NezhaGuard] {} 告警 — {}", type_str, ip);

    std::string sev_str = [](Severity s) -> std::string {
        switch (s) {
            case Severity::Critical: return "致命";
            case Severity::Error:    return "严重";
            case Severity::Warn:     return "警告";
            default:                       return "信息";
        }
    }(alert.level);

    std::string body = std::format(
        "**类型**: {}\n"
        "**等级**: {}\n"
        "**来源**: {}\n"
        "**频次**: {} 次\n"
        "**评分**: {:.1f}\n"
        "**证据**: {}\n"
        "**详情**: {}",
        type_str, sev_str, ip, alert.count, alert.score, evidence, detail
    );

    std::lock_guard<std::mutex> lock(mtx_);

    for (const auto &rule: rules_) {
        if (alert.level < rule.min_level) continue;
        if (!keyword_match(combined, rule.keywords)) continue;

        NZ_INFO("[通知] 触发规则: {} -> {}",
                rule.keywords.empty() ? "*" : rule.keywords[0],
                rule.channels.empty() ? "?" : channel_name(rule.channels[0]));

        dispatch(rule, title, body);
    }
}

// -- 分发 ----------------------------------------------------------------
void Notifier::dispatch(const TriggerRule &rule, const std::string &title, const std::string &body) {
    for (auto ch: rule.channels) {
        auto it = channels_.find(ch);
        if (it == channels_.end() || !it->second.enabled) {
            if (ch == NotifyChannel::LocalGui) {
                send_local_gui(title, body);
            }
            continue;
        }

        const auto &cfg = it->second;
        // 后台线程发送，不阻塞主线程
        std::thread([this, ch, url = cfg.webhook_url, title, body]() {
            switch (ch) {
                case NotifyChannel::Slack:    send_slack(url, body); break;
                case NotifyChannel::Discord:  send_discord(url, body); break;
                case NotifyChannel::DingTalk: send_dingtalk(url, body); break;
                case NotifyChannel::Feishu:   send_feishu(url, body); break;
                case NotifyChannel::WeChat:   send_wechat(url, body); break;
                case NotifyChannel::Email:    send_email(url, title, body); break;
                default: break;
            }
        }).detach();
    }
}

// -- HTTP 工具 ----------------------------------------------------------------
std::string Notifier::http_post(const std::string &url, const std::string &json) {
    std::string cmd = std::format(
        "curl -s --max-time 5 -X POST \"{}\" -H \"Content-Type: application/json\" -d '{}' 2>/dev/null",
        url, json
    );
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return {};

    std::string out;
    char buf[4096];
    while (std::fgets(buf, sizeof(buf), pipe.get())) out += buf;
    return out;
}

// -- Slack ----------------------------------------------------------------
void Notifier::send_slack(const std::string &webhook, const std::string &msg) {
    std::string escaped = msg;
    for (std::size_t i = 0; i < escaped.size(); ++i) {
        if (escaped[i] == '"') { escaped.insert(i, "\\"); ++i; }
        if (escaped[i] == '\n') { escaped.replace(i, 1, "\\n"); ++i; }
    }

    std::string json = std::format(
        "{{\"text\":\"{}\"}}", escaped
    );
    std::string resp = http_post(webhook, json);
    NZ_DEBUG("[通知] Slack 发送结果: {}", resp.empty() ? "ok" : resp);
}

// -- Discord ----------------------------------------------------------------
void Notifier::send_discord(const std::string &webhook, const std::string &msg) {
    std::string escaped = msg;
    for (std::size_t i = 0; i < escaped.size(); ++i) {
        if (escaped[i] == '"') { escaped.insert(i, "\\"); ++i; }
        if (escaped[i] == '\n') { escaped.replace(i, 1, "\\n"); ++i; }
    }

    std::string json = std::format(
        "{{\"embeds\":[{{\"title\":\"NezhaGuard 安全告警\",\"description\":\"{}\","
        "\"color\":16711680}}]}}", escaped
    );
    std::string resp = http_post(webhook, json);
    NZ_DEBUG("[通知] Discord 发送结果: {}", resp.empty() ? "ok" : resp);
}

// -- 钉钉 ----------------------------------------------------------------
void Notifier::send_dingtalk(const std::string &webhook, const std::string &msg) {
    std::string escaped = msg;
    for (std::size_t i = 0; i < escaped.size(); ++i) {
        if (escaped[i] == '"') { escaped.insert(i, "\\"); ++i; }
        if (escaped[i] == '\n') { escaped.replace(i, 1, "\\n"); ++i; }
    }

    std::string json = std::format(
        "{{\"msgtype\":\"markdown\",\"markdown\":{{\"title\":\"NezhaGuard 安全告警\",\"text\":\"{}\"}}}}",
        escaped
    );
    std::string resp = http_post(webhook, json);
    NZ_DEBUG("[通知] DingTalk 发送结果: {}", resp.empty() ? "ok" : resp);
}

// -- 飞书 ----------------------------------------------------------------
void Notifier::send_feishu(const std::string &webhook, const std::string &msg) {
    std::string escaped = msg;
    for (std::size_t i = 0; i < escaped.size(); ++i) {
        if (escaped[i] == '"') { escaped.insert(i, "\\"); ++i; }
        if (escaped[i] == '\n') { escaped.replace(i, 1, "\\n"); ++i; }
    }

    std::string json = std::format(
        "{{\"msg_type\":\"interactive\",\"card\":{{\"header\":{{\"title\":{{\"tag\":\"plain_text\","
        "\"content\":\"NezhaGuard 安全告警\"}},\"template\":\"red\"}},\"elements\":[{{\"tag\":\"markdown\","
        "\"content\":\"{}\"}}]}}}}", escaped
    );
    std::string resp = http_post(webhook, json);
    NZ_DEBUG("[通知] 飞书 发送结果: {}", resp.empty() ? "ok" : resp);
}

// -- 企业微信 ----------------------------------------------------------------
void Notifier::send_wechat(const std::string &webhook, const std::string &msg) {
    std::string escaped = msg;
    for (std::size_t i = 0; i < escaped.size(); ++i) {
        if (escaped[i] == '"') { escaped.insert(i, "\\"); ++i; }
        if (escaped[i] == '\n') { escaped.replace(i, 1, "\\n"); ++i; }
    }

    std::string json = std::format(
        "{{\"msgtype\":\"markdown\",\"markdown\":{{\"content\":\"{}\"}}}}", escaped
    );
    std::string resp = http_post(webhook, json);
    NZ_DEBUG("[通知] WeChat 发送结果: {}", resp.empty() ? "ok" : resp);
}

// -- 邮件 ----------------------------------------------------------------
void Notifier::send_email(const std::string &to, const std::string &subject, const std::string &body) {
#if defined(__APPLE__)
    std::string cmd = std::format(
        "echo '{}' | mail -s '{}' '{}' 2>/dev/null", body, subject, to
    );
#else
    std::string cmd = std::format(
        "echo '{}' | mail -s '{}' '{}' 2>/dev/null", body, subject, to
    );
#endif
    std::system(cmd.c_str());
    NZ_DEBUG("[通知] 邮件 发送至: {}", to);
}

// -- 本地 GUI 弹窗 ----------------------------------------------------------------
void Notifier::send_local_gui(const std::string &title, const std::string &body) {
    if (gui_callback_) {
        gui_callback_(title, body);
        return;
    }

#if defined(__APPLE__)
    std::string escaped_title = title;
    std::string escaped_body = body;
    for (std::size_t i = 0; i < escaped_title.size(); ++i)
        if (escaped_title[i] == '"') { escaped_title.insert(i, "\\"); ++i; }
    for (std::size_t i = 0; i < escaped_body.size(); ++i)
        if (escaped_body[i] == '"') { escaped_body.insert(i, "\\"); ++i; }

    std::string cmd = std::format(
        "osascript -e 'display notification \"{}\" with title \"{}\" sound name \"Glass\"' 2>/dev/null",
        escaped_body, escaped_title
    );
    std::system(cmd.c_str());
#elif defined(__linux__)
    std::string cmd = std::format(
        "notify-send '{}' '{}' 2>/dev/null", title, body
    );
    std::system(cmd.c_str());
#endif
    NZ_DEBUG("[通知] 本地推送: {}", title);
}

} // namespace Nezha::Service
