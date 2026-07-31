#include "tor_checker.h"
#include "../utilities/logger.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace Nezha::Core {

    static constexpr const char *kCacheFile = "tor_nodes.cache";
    static constexpr const char *kFetchURL =
        "https://check.torproject.org/torbulkexitlist";

    bool TorChecker::initialize() {
        load_from_cache();
        if (nodes_.empty()) {
            NZ_INFO("TorChecker: 本地缓存为空，正在从 Tor Project 获取列表...");
            if (fetch_from_tor_project()) {
                save_to_cache();
                NZ_INFO("TorChecker: 已加载 {} 个 Tor 出口节点", nodes_.size());
            } else {
                NZ_WARN("TorChecker: 无法获取 Tor 出口节点列表，Tor 检测功能不可用");
                return false;
            }
        } else {
            NZ_INFO("TorChecker: 从缓存加载了 {} 个 Tor 出口节点", nodes_.size());
            std::thread([this]() {
                if (fetch_from_tor_project()) save_to_cache();
            }).detach();
        }
        return !nodes_.empty();
    }

    bool TorChecker::is_tor_exit(const std::string &ip) const {
        return nodes_.count(ip) > 0;
    }

    void TorChecker::refresh() {
        NZ_INFO("TorChecker: 正在刷新 Tor 出口节点列表...");
        if (fetch_from_tor_project()) {
            save_to_cache();
            NZ_INFO("TorChecker: 刷新完成，共 {} 个节点", nodes_.size());
        }
    }

    void TorChecker::load_from_cache() {
        std::ifstream f(kCacheFile);
        if (!f.is_open()) return;
        nodes_.clear();
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line[0] != '#')
                nodes_.insert(line);
        }
    }

    void TorChecker::save_to_cache() {
        std::ofstream f(kCacheFile, std::ios::trunc);
        if (!f.is_open()) return;
        for (const auto &ip : nodes_)
            f << ip << '\n';
    }

    bool TorChecker::fetch_from_tor_project() {
        std::string cmd = "curl -s --max-time 30 " + std::string(kFetchURL) + " 2>/dev/null";
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) return false;

        std::unordered_set<std::string> fresh;
        char buf[64];
        while (fgets(buf, sizeof(buf), pipe.get())) {
            std::size_t len = std::strlen(buf);
            while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
                buf[--len] = '\0';
            if (len >= 7 && len <= 39) {
                bool valid = true;
                for (std::size_t i = 0; i < len; ++i) {
                    if (buf[i] != '.' && !(buf[i] >= '0' && buf[i] <= '9'))
                    { valid = false; break; }
                }
                if (valid) fresh.insert(std::string(buf, len));
            }
        }
        if (fresh.empty()) return false;
        nodes_ = std::move(fresh);
        return true;
    }

}
