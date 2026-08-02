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
#include <filesystem>
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
        case NotifyChannel::Telegram: return "Telegram";
        default:                      return "???";
    }
}

NotifyChannel channel_from_name(const std::string &name) {
    if (name == "slack")    return NotifyChannel::Slack;
    if (name == "discord")  return NotifyChannel::Discord;
    if (name == "wechat")   return NotifyChannel::WeChat;
    if (name == "dingtalk") return NotifyChannel::DingTalk;
    if (name == "email")    return NotifyChannel::Email;
    if (name == "local")    return NotifyChannel::LocalGui;
    if (name == "feishu")   return NotifyChannel::Feishu;
    if (name == "telegram") return NotifyChannel::Telegram;
    return NotifyChannel::LocalGui; // fallback
}

Notifier &Notifier::instance() noexcept {
    static Notifier inst;
    return inst;
}

static std::string trim(const std::string &s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::vector<std::string> split_csv(const std::string &s) {
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        auto t = trim(item);
        if (!t.empty()) out.push_back(t);
    }
    return out;
}

static Severity parse_severity(const std::string &s) {
    auto t = trim(s);
    std::string lower = t;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (lower == "critical" || lower == "crit") return Severity::Critical;
    if (lower == "error" || lower == "err")     return Severity::Error;
    if (lower == "warn" || lower == "warning")  return Severity::Warn;
    if (lower == "info")                        return Severity::Info;
    if (lower == "debug")                       return Severity::Debug;
    return Severity::Warn;
}

void Notifier::configure_channel(const ChannelConfig &cfg) {
    std::lock_guard<std::mutex> lock(mtx_);
    channels_[cfg.channel] = cfg;
    if (cfg.enabled)
        NZ_INFO("[通知] {} — 已启用, 关键词: {} 个", channel_name(cfg.channel), cfg.keywords.size());
}

int Notifier::load_config_dir(const std::string &dir_path) {
    namespace fs = std::filesystem;
    std::error_code ec;

    if (!fs::is_directory(dir_path, ec)) {
        NZ_INFO("[通知] 配置目录不存在: {}", dir_path);
        return 0;
    }

    int loaded = 0;
    for (const auto &entry: fs::directory_iterator(dir_path, ec)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".conf") continue;

        std::string stem = entry.path().stem().string();
        NotifyChannel ch = channel_from_name(stem);

        std::ifstream f(entry.path());
        if (!f.is_open()) continue;

        ChannelConfig cfg;
        cfg.channel = ch;
        cfg.enabled = false;

        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;

            auto eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string key = trim(line.substr(0, eq));
            std::string val = trim(line.substr(eq + 1));

            if (key == "webhook" || key == "url")
                cfg.webhook_url = val;
            else if (key == "chat_id")
                cfg.chat_id = val;
            else if (key == "enabled")
                cfg.enabled = (val == "true" || val == "1" || val == "yes");
            else if (key == "keywords")
                cfg.keywords = split_csv(val);
            else if (key == "min_severity" || key == "min_level")
                cfg.min_level = parse_severity(val);
        }

        if (cfg.enabled && !cfg.webhook_url.empty() && !cfg.chat_id.empty()) {
            // Telegram 需要 token + chat_id
            configure_channel(cfg);
            ++loaded;
        } else if (cfg.enabled && !cfg.webhook_url.empty()) {
            configure_channel(cfg);
            ++loaded;
        } else if (cfg.enabled && ch == NotifyChannel::LocalGui) {
            configure_channel(cfg);
            ++loaded;
        } else if (cfg.enabled && ch == NotifyChannel::Email) {
            configure_channel(cfg);
            ++loaded;
        } else if (!cfg.webhook_url.empty() && !cfg.chat_id.empty()) {
            configure_channel(cfg);
            ++loaded;
        } else if (!cfg.webhook_url.empty()) {
            configure_channel(cfg);
            ++loaded;
        }
    }

    NZ_INFO("[通知] 已加载 {} 个渠道配置", loaded);
    return loaded;
}

void Notifier::set_gui_callback(std::function<void(const std::string &, const std::string &)> cb) {
    std::lock_guard<std::mutex> lock(mtx_);
    gui_callback_ = std::move(cb);
}

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
            default:                 return "信息";
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

    for (const auto &[ch, cfg]: channels_) {
        if (!cfg.enabled) continue;
        if (alert.level < cfg.min_level) continue;
        if (cfg.keywords.empty()) continue;
        if (!keyword_match(combined, cfg.keywords)) continue;

        NZ_INFO("[通知] 触发: {} -> {}", channel_name(ch), cfg.keywords[0]);
        dispatch(cfg, title, body);
    }
}

void Notifier::dispatch(const ChannelConfig &cfg, const std::string &title, const std::string &body) {
    auto ch = cfg.channel;
    std::string url = cfg.webhook_url;
    std::string chat = cfg.chat_id;

    std::thread([this, ch, url, chat, title, body]() {
        switch (ch) {
            case NotifyChannel::Slack:    send_slack(url, body); break;
            case NotifyChannel::Discord:  send_discord(url, body); break;
            case NotifyChannel::DingTalk: send_dingtalk(url, body); break;
            case NotifyChannel::Feishu:   send_feishu(url, body); break;
            case NotifyChannel::WeChat:   send_wechat(url, body); break;
            case NotifyChannel::Email:    send_email(url, title, body); break;
            case NotifyChannel::LocalGui: send_local_gui(title, body); break;
            case NotifyChannel::Telegram: send_telegram(url, chat, body); break;
        }
    }).detach();
}

std::string Notifier::http_post(const std::string &url, const std::string &json) {
    const std::string cmd = std::format(
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

static std::string escape_json(const std::string &s) {
    std::string out;
    for (char c: s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

void Notifier::send_slack(const std::string &webhook, const std::string &msg) {
    std::string json = std::format(
        "{{\"text\":\"{}\"}}", escape_json(msg)
    );
    std::string resp = http_post(webhook, json);
    NZ_DEBUG("[通知] Slack 结果: {}", resp.empty() ? "ok" : resp);
}

void Notifier::send_discord(const std::string &webhook, const std::string &msg) {
    std::string json = std::format(
        "{{\"embeds\":[{{\"title\":\"NezhaGuard 安全告警\",\"description\":\"{}\","
        "\"color\":16711680}}]}}", escape_json(msg)
    );
    std::string resp = http_post(webhook, json);
    NZ_DEBUG("[通知] Discord 结果: {}", resp.empty() ? "ok" : resp);
}

void Notifier::send_dingtalk(const std::string &webhook, const std::string &msg) {
    std::string json = std::format(
        "{{\"msgtype\":\"markdown\",\"markdown\":{{\"title\":\"NezhaGuard 安全告警\",\"text\":\"{}\"}}}}",
        escape_json(msg)
    );
    std::string resp = http_post(webhook, json);
    NZ_DEBUG("[通知] DingTalk 结果: {}", resp.empty() ? "ok" : resp);
}

void Notifier::send_feishu(const std::string &webhook, const std::string &msg) {
    std::string json = std::format(
        "{{\"msg_type\":\"interactive\",\"card\":{{\"header\":{{\"title\":{{\"tag\":\"plain_text\","
        "\"content\":\"NezhaGuard 安全告警\"}},\"template\":\"red\"}},\"elements\":[{{\"tag\":\"markdown\","
        "\"content\":\"{}\"}}]}}}}", escape_json(msg)
    );
    std::string resp = http_post(webhook, json);
    NZ_DEBUG("[通知] 飞书 结果: {}", resp.empty() ? "ok" : resp);
}

void Notifier::send_wechat(const std::string &webhook, const std::string &msg) {
    std::string json = std::format(
        "{{\"msgtype\":\"markdown\",\"markdown\":{{\"content\":\"{}\"}}}}", escape_json(msg)
    );
    std::string resp = http_post(webhook, json);
    NZ_DEBUG("[通知] WeChat 结果: {}", resp.empty() ? "ok" : resp);
}

void Notifier::send_email(const std::string &to, const std::string &subject, const std::string &body) {
    std::string cmd = std::format(
        "echo '{}' | mail -s '{}' '{}' 2>/dev/null", body, subject, to
    );
    std::system(cmd.c_str());
    NZ_DEBUG("[通知] 邮件 发送至: {}", to);
}

void Notifier::send_local_gui(const std::string &title, const std::string &body) {
    if (gui_callback_) {
        gui_callback_(title, body);
        return;
    }

#if defined(__APPLE__)
    std::string cmd = std::format(
        "osascript -e 'display notification \"{}\" with title \"{}\" sound name \"Glass\"' 2>/dev/null",
        escape_json(body), escape_json(title)
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

void Notifier::send_telegram(const std::string &token, const std::string &chat, const std::string &msg) {
    std::string url = std::format(
        "https://api.telegram.org/bot{}/sendMessage", token);
    std::string json = std::format(
        "{{\"chat_id\":\"{}\",\"text\":\"{}\",\"parse_mode\":\"Markdown\"}}",
        chat, escape_json(msg));
    std::string resp = http_post(url, json);
    NZ_DEBUG("[通知] Telegram 结果: {}", resp.empty() ? "ok" : resp);
}

} // namespace Nezha::Service
