//
// Created by 钟智强 on 2026/8/2.
//

#include "ipcn.h"
#include "../utilities/logger.h"

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <regex>

namespace Nezha::Core {

std::mutex IpCn::ticket_mtx_;
std::string IpCn::cached_ticket_;
std::chrono::steady_clock::time_point IpCn::ticket_expiry_;

namespace {
    constexpr const char *kHomePageURL = "https://www.ip.cn/";
    constexpr const char *kAPIBase = "https://my.ip.cn/json/?ticket=";

    bool is_private_ip(const std::string &ip) {
        if (ip.rfind("192.168.", 0) == 0) return true;
        if (ip.rfind("10.", 0) == 0) return true;
        if (ip.rfind("172.16.", 0) == 0) return true;
        if (ip == "127.0.0.1" || ip == "0.0.0.0") return true;
        if (ip.rfind("169.254.", 0) == 0) return true;
        return false;
    }

    std::string http_get(const std::string &url) {
        std::string cmd = std::string("curl -s --max-time 5 \"") + url + "\" 2>/dev/null";
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) return {};

        std::string out;
        char buf[4096];
        while (std::fgets(buf, sizeof(buf), pipe.get())) out += buf;
        return out;
    }

    std::string extract_json_str(const std::string &json, const char *key) {
        std::string q = std::string("\"") + key + "\":\"";
        auto pos = json.find(q);
        if (pos == std::string::npos) return {};
        pos += q.size();
        auto end = json.find('"', pos);
        if (end == std::string::npos) return {};
        return json.substr(pos, end - pos);
    }
}

bool IpCn::fetch_ticket(std::string &ticket_out) {
    {
        std::lock_guard<std::mutex> lock(ticket_mtx_);
        auto now = std::chrono::steady_clock::now();
        if (!cached_ticket_.empty() && now < ticket_expiry_) {
            ticket_out = cached_ticket_;
            return true;
        }
    }

    std::string html = http_get(kHomePageURL);
    if (html.empty()) return false;

    std::regex re("var _ticket = \"([^\"]+)\"");
    std::smatch match;
    if (!std::regex_search(html, match, re)) return false;

    {
        std::lock_guard<std::mutex> lock(ticket_mtx_);
        cached_ticket_ = match[1].str();
        ticket_expiry_ = std::chrono::steady_clock::now() + std::chrono::minutes(5);
        ticket_out = cached_ticket_;
    }
    return true;
}

IpCnResult IpCn::parse(const std::string &json) {
    IpCnResult r{};
    if (json.find("\"status\":true") == std::string::npos &&
        json.find("\"status\": true") == std::string::npos)
        return r;

    auto data_pos = json.find("\"data\":");
    if (data_pos == std::string::npos) return r;
    std::string data = json.substr(data_pos);

    r.ip = extract_json_str(data, "ip");
    r.country = extract_json_str(data, "country");
    r.province = extract_json_str(data, "province");
    r.city = extract_json_str(data, "city");
    r.district = extract_json_str(data, "district");
    r.isp = extract_json_str(data, "isp");
    r.valid = !r.ip.empty();
    return r;
}

IpCnResult IpCn::lookup_self() {
    IpCnResult r{};
    std::string ticket;
    if (!fetch_ticket(ticket)) {
        NZ_DEBUG("IpCn: 无法获取 ticket");
        return r;
    }

    std::string url = std::string(kAPIBase) + ticket;
    std::string json = http_get(url);
    if (json.empty()) return r;

    r = parse(json);
    if (r.valid)
        NZ_DEBUG("IpCn: 本机公网IP = {} ({}, {}, {})", r.ip, r.country, r.province, r.isp);
    return r;
}

IpCnResult IpCn::lookup(const std::string &ip) {
    IpCnResult r{};
    if (ip.empty()) return r;

    if (is_private_ip(ip)) {
        r.ip = ip;
        r.country = "局域网";
        r.isp = "本地网络";
        r.valid = true;
        return r;
    }

    struct sockaddr_in sa;
    if (::inet_pton(AF_INET, ip.c_str(), &sa.sin_addr) != 1) return {};

    std::string ticket;
    if (!fetch_ticket(ticket)) return r;

    std::string url = std::string(kAPIBase) + ticket + "&ip=" + ip;
    std::string json = http_get(url);
    if (json.empty()) return r;

    r = parse(json);
    return r;
}

} // namespace Nezha::Core
