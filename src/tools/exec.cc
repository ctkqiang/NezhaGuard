//
// Created by 钟智强 on 2026/8/7.
//

#include "exec.h"

#include <chrono>
#include <cstdio>
#include <memory>
#include <poll.h>
#include <unistd.h>

namespace Nezha::Tools {

std::string run_command(const std::string &cmd, int timeout_ms) {
    std::string full_cmd = cmd + " 2>&1";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(full_cmd.c_str(), "r"), pclose);
    if (!pipe) return {};

    int fd = fileno(pipe.get());
    std::string out;
    char buf[8192];
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) break;
        int remaining = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());

        struct pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;
        int ret = poll(&pfd, 1, remaining);
        if (ret < 0) break;
        if (ret == 0) break; // timeout

        auto n = fread(buf, 1, sizeof(buf), pipe.get());
        if (n <= 0) break;
        out.append(buf, n);
    }

    return out;
}

} // namespace Nezha::Tools