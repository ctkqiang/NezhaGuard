//
// Created by 钟智强 on 2026/8/2.
//

#include "application_monitor.h"
#include "../utilities/logger.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>

namespace Nezha::Core {

ApplicationMonitor::~ApplicationMonitor() { stop(); }

void ApplicationMonitor::add_app(const MonitoredApp &app) {
    apps_.push_back(app);
    states_[app.port] = AppState{};
}

int ApplicationMonitor::load_from_file(const std::string &path) {
    std::ifstream f(path);
    if (!f.is_open()) return -1;

    int loaded = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        MonitoredApp app;
        if (!(iss >> app.name >> app.port)) continue;

        std::string log_path;
        while (iss >> log_path)
            app.log_paths.push_back(log_path);

        add_app(app);
        ++loaded;
    }
    return loaded;
}

void ApplicationMonitor::start() {
    if (running_) return;
    running_ = true;
    worker_ = std::thread([this]() { monitor_loop(); });
}

void ApplicationMonitor::stop() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

bool ApplicationMonitor::check_port(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        close(sock);
        return false;
    }

    int ret = connect(sock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    close(sock);
    return ret == 0;
}

void ApplicationMonitor::monitor_loop() {
    using namespace std::chrono;

    NZ_INFO("[应用监控] 已启动, 监控 {} 个应用", apps_.size());
    for (const auto &app: apps_)
        NZ_INFO("[应用监控]   {} — 端口 {} — 日志源 {} 个",
                app.name, app.port, app.log_paths.size());

    while (running_) {
        for (auto &app: apps_) {
            bool now_online = check_port(app.port);
            auto &state = states_[app.port];

            if (now_online && !state.online) {
                NZ_INFO("[应用监控] {} :{} — 已上线", app.name, app.port);

                state.online = true;
                state.files.clear();

                for (const auto &path: app.log_paths) {
                    FileState fs;
                    fs.path = path;
                    fs.stream.open(path);
                    if (fs.stream.is_open()) {
                        fs.stream.seekg(0, std::ios::end);
                        fs.last_pos = fs.stream.tellg();
                        NZ_INFO("[应用监控] {} — 正在监控日志: {}", app.name, path);
                    }
                    state.files.push_back(std::move(fs));
                }
            } else if (!now_online && state.online) {
                NZ_INFO("[应用监控] {} :{} — 已下线", app.name, app.port);
                state.online = false;
                state.files.clear();
            }

            if (state.online) {
                for (auto &fs: state.files) {
                    if (!fs.stream.is_open()) {
                        fs.stream.open(fs.path);
                        if (fs.stream.is_open()) {
                            fs.stream.seekg(0, std::ios::end);
                            fs.last_pos = fs.stream.tellg();
                        }
                        continue;
                    }
                    fs.stream.clear();
                    fs.stream.seekg(0, std::ios::end);
                    auto cur_pos = fs.stream.tellg();
                    if (cur_pos < fs.last_pos) {
                        fs.stream.clear();
                        fs.stream.seekg(0, std::ios::beg);
                        fs.last_pos = 0;
                        cur_pos = fs.stream.tellg();
                    }
                    if (cur_pos > fs.last_pos) {
                        fs.stream.seekg(fs.last_pos);
                        std::string line;
                        while (std::getline(fs.stream, line) && running_) {
                            if (line.empty()) continue;
                            NZ_INFO("[{}] {}", app.name, line);
                        }
                        fs.stream.clear();
                        fs.last_pos = fs.stream.tellg();
                    }
                }
            }
        }

        auto next = steady_clock::now() + seconds(5);
        while (running_ && steady_clock::now() < next)
            std::this_thread::sleep_for(milliseconds(200));
    }

    NZ_INFO("[应用监控] 已停止");
}

} // namespace Nezha::Core
