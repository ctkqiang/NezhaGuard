//
// Created by 钟智强 on 2026/8/2.
//

#ifndef NEZHAGUARD_APPLICATION_MONITOR_H
#define NEZHAGUARD_APPLICATION_MONITOR_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <unordered_map>

namespace Nezha::Core {

struct MonitoredApp {
    std::string name;
    int port = 0;
    std::vector<std::string> log_paths;
};

class ApplicationMonitor {
public:
    ApplicationMonitor() = default;
    ~ApplicationMonitor();

    ApplicationMonitor(const ApplicationMonitor &) = delete;
    ApplicationMonitor &operator=(const ApplicationMonitor &) = delete;

    void add_app(const MonitoredApp &app);
    int load_from_file(const std::string &path);
    void start();
    void stop();

    [[nodiscard]] bool running() const noexcept { return running_; }
    [[nodiscard]] const std::vector<MonitoredApp> &apps() const noexcept { return apps_; }

private:
    void monitor_loop();
    static bool check_port(int port);

    struct FileState {
        std::string path;
        std::ifstream stream;
        std::streampos last_pos = 0;
    };

    struct AppState {
        bool online = false;
        std::vector<FileState> files;
    };

    std::vector<MonitoredApp> apps_;
    std::unordered_map<int, AppState> states_;
    std::thread worker_;
    std::atomic<bool> running_{false};
};

} // namespace Nezha::Core

#endif //NEZHAGUARD_APPLICATION_MONITOR_H
