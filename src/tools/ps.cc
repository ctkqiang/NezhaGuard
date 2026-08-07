//
// Created by 钟智强 on 2026/8/7.
//

#include "ps.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <libproc.h>
#include <pwd.h>
#elif defined(__linux__)
#include <dirent.h>
#include <pwd.h>
#endif

namespace Nezha::Tools {

ToolResult PsTool::run(const ToolOptions &opts) const {
    auto t0 = std::chrono::steady_clock::now();
    ToolResult result;
    result.id = ToolId::Ps;
    result.tool = "ps";
    result.columns = {"PID", "PPID", "用户", "状态", "内存(MB)", "命令"};

    std::vector<std::vector<std::string>> rows;

#if defined(__linux__)
    DIR *proc = opendir("/proc");
    if (!proc) {
        result.ok = false;
        result.error = "无法访问 /proc";
        return result;
    }

    std::vector<int> pids;
    struct dirent *de;
    while ((de = readdir(proc)) != nullptr) {
        if (de->d_type == DT_DIR && std::all_of(de->d_name, de->d_name + strlen(de->d_name),
                                                 [](char c) { return c >= '0' && c <= '9'; }))
            pids.push_back(std::stoi(de->d_name));
    }
    closedir(proc);

    std::sort(pids.begin(), pids.end());

    int limit = std::min(opts.limit, static_cast<int>(pids.size()));
    for (int i = 0; i < limit; ++i) {
        int pid = pids[i];
        std::string stat_path = "/proc/" + std::to_string(pid) + "/stat";
        std::ifstream sf(stat_path);
        if (!sf.is_open()) continue;
        std::string line;
        std::getline(sf, line);

        // parse /proc/[pid]/stat
        int ppid = 0, rss = 0;
        char state = '?';
        char comm[256] = {};
        auto rp = line.find(')');
        if (rp == std::string::npos) continue;
        sscanf(line.c_str() + rp + 2, "%c %d", &state, &ppid);
        auto fields = line.substr(rp + 2);
        std::istringstream fs(fields);
        std::string f;
        for (int j = 0; j < 22 && fs >> f; ++j) {
            if (j == 21) rss = std::stoi(f); // RSS field (22nd after ')')
        }

        // cmdline
        std::ifstream cf("/proc/" + std::to_string(pid) + "/cmdline");
        std::string cmd;
        if (cf.is_open()) {
            std::getline(cf, cmd, '\0');
            if (cmd.empty()) {
                std::ifstream sf2("/proc/" + std::to_string(pid) + "/comm");
                if (sf2.is_open()) std::getline(sf2, cmd);
            }
        }
        if (cmd.empty()) cmd = "[" + std::string(comm) + "]";

        // uid
        std::ifstream uf("/proc/" + std::to_string(pid) + "/status");
        int uid = 0;
        if (uf.is_open()) {
            std::string uline;
            while (std::getline(uf, uline)) {
                if (uline.starts_with("Uid:")) {
                    sscanf(uline.c_str() + 4, "%d", &uid);
                    break;
                }
            }
        }

        std::string user = std::to_string(uid);
        if (auto *pw = getpwuid(uid)) user = pw->pw_name;

        std::ranges::replace(cmd, std::string("\0", 1), std::string(" "));
        rows.push_back({std::to_string(pid), std::to_string(ppid), user,
                        std::string(1, state),
                        std::to_string(rss * getpagesize() / 1048576.0).substr(0, 5),
                        cmd});
    }

#elif defined(__APPLE__)
    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t sz = 0;
    if (sysctl(mib, 4, nullptr, &sz, nullptr, 0) < 0) {
        result.ok = false;
        result.error = "sysctl KERN_PROC_ALL 失败";
        return result;
    }

    auto *buf = static_cast<char *>(std::malloc(sz));
    if (sysctl(mib, 4, buf, &sz, nullptr, 0) < 0) {
        std::free(buf);
        result.ok = false;
        result.error = "sysctl KERN_PROC_ALL 失败";
        return result;
    }

    struct kinfo_proc *kp = reinterpret_cast<kinfo_proc *>(buf);
    int count = static_cast<int>(sz / sizeof(kinfo_proc));
    int limit = std::min(opts.limit, count);

    for (int i = 0; i < limit; ++i) {
        if (kp[i].kp_proc.p_pid == 0) continue;
        int pid = kp[i].kp_proc.p_pid;
        int ppid = kp[i].kp_eproc.e_ppid;
        int uid = kp[i].kp_eproc.e_ucred.cr_uid;

        std::string user = std::to_string(uid);
        if (auto *pw = getpwuid(uid)) user = pw->pw_name;

        std::string state = "?";
        switch (kp[i].kp_proc.p_stat) {
            case SRUN: state = "R"; break;
            case SSLEEP: state = "S"; break;
            case SZOMB: state = "Z"; break;
            case SSTOP: state = "T"; break;
        }

        struct proc_taskinfo ti{};
        int rss_mb = 0;
        if (proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &ti, sizeof(ti)) > 0)
            rss_mb = static_cast<int>(ti.pti_resident_size / 1048576);

        rows.push_back({std::to_string(pid), std::to_string(ppid), user,
                        state, std::to_string(rss_mb),
                        std::string(kp[i].kp_proc.p_comm)});
    }
    std::free(buf);
#endif

    result.rows = std::move(rows);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);
    result.elapsed_ms = elapsed;
    result.ok = true;
    result.summary = std::to_string(result.rows.size()) + " 进程 (用时 " +
                     std::to_string(elapsed.count()) + "ms)";
    return result;
}

} // namespace Nezha::Tools