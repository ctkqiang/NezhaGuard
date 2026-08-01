#include "geo_ip.h"
#include "../utilities/logger.h"

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace Nezha::Core {

    namespace {

        constexpr const char *kAPIURL = "http://ip-api.com/json/%s?fields=66846719&lang=zh-CN";

        std::mutex g_geo_mutex;
        std::unordered_map<std::string, GeoIPResult> g_geo_cache;

        bool is_private_ip(const std::string &ip) {
            if (ip.rfind("192.168.", 0) == 0) return true;
            if (ip.rfind("10.", 0) == 0) return true;
            if (ip.rfind("172.16.", 0) == 0) return true;
            if (ip == "127.0.0.1" || ip == "0.0.0.0") return true;
            if (ip.rfind("169.254.", 0) == 0) return true;
            return false;
        }

        double parse_double(const char *s) {
            if (!s || !*s) return 0.0;
            return std::strtod(s, nullptr);
        }

        std::string extract_json_str(const std::string &json, const char *key) {
            std::string q = std::string("\"") + key + "\":\"";
            auto pos = json.find(q);
            if (pos == std::string::npos) {
                q = std::string("\"") + key + "\":";
                pos = json.find(q);
                if (pos == std::string::npos) return {};
                pos += q.size();
                auto end = json.find(',', pos);
                if (end == std::string::npos) end = json.find('}', pos);
                return json.substr(pos, end - pos);
            }
            pos += q.size();
            auto end = json.find('"', pos);
            return json.substr(pos, end - pos);
        }

        GeoIPResult parse_json(const std::string &json) {
            GeoIPResult r{};
            r.country = extract_json_str(json, "country");
            r.country_code = extract_json_str(json, "countryCode");
            r.region = extract_json_str(json, "regionName");
            r.city = extract_json_str(json, "city");
            r.lat = parse_double(extract_json_str(json, "lat").c_str());
            r.lon = parse_double(extract_json_str(json, "lon").c_str());
            r.isp = extract_json_str(json, "isp");
            r.org = extract_json_str(json, "org");
            r.timezone = extract_json_str(json, "timezone");
            r.valid = !r.country.empty();
            return r;
        }

        GeoIPResult fetch_from_api(const std::string &ip) {
            /* 防御: 严格校验 IP 格式，防止命令注入 */
            struct sockaddr_in sa;
            if (::inet_pton(AF_INET, ip.c_str(), &sa.sin_addr) != 1) return {};
            if (is_private_ip(ip)) return {};

            char url[512];
            std::snprintf(url, sizeof(url), kAPIURL, ip.c_str());
            std::string cmd = std::string("curl -s --max-time 5 \"") + url + "\" 2>/dev/null";
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
            if (!pipe) return {};

            std::string json;
            char buf[4096];
            while (std::fgets(buf, sizeof(buf), pipe.get())) json += buf;
            if (json.find("\"status\":\"success\"") == std::string::npos) return {};

            GeoIPResult r = parse_json(json);
            r.ip = ip;
            return r;
        }

    }

    GeoIPResult GeoIP::lookup(const std::string &ip) {
        if (ip.empty()) return {};

        if (is_private_ip(ip)) {
            GeoIPResult r{};
            r.ip = ip;
            r.country = "\xe5\xb1\x80\xe5\x9f\x9f\xe7\xbd\x91"; // 局域网
            r.country_code = "LAN";
            r.city = "\xe6\x9c\xac\xe5\x9c\xb0\xe7\xbd\x91\xe7\xbb\x9c"; // 本地网络
            r.valid = true;
            return r;
        }

        {
            std::lock_guard<std::mutex> lock(g_geo_mutex);
            if (auto it = g_geo_cache.find(ip); it != g_geo_cache.end())
                return it->second;
        }

        NZ_DEBUG("GeoIP: 查询 {}", ip);
        auto result = fetch_from_api(ip);
        if (result.valid) {
            std::lock_guard<std::mutex> lock(g_geo_mutex);
            g_geo_cache[ip] = result;
        }
        return result;
    }

} // namespace Nezha::Core
