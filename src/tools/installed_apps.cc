//
// Created by 钟智强 on 2026/8/8.
//

#include "installed_apps.h"
#include "exec.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>

namespace Nezha::Tools {

ToolResult InstalledAppsTool::run(const ToolOptions &opts) const {
    auto t0 = std::chrono::steady_clock::now();
    ToolResult result;
    result.id = ToolId::InstalledApps;
    result.tool = "installed_apps";
    result.columns = {"名称", "版本", "来源", "路径"};

    std::vector<std::vector<std::string>> rows;
    int limit = std::max(1, std::min(opts.limit, 2000));
    int count = 0;

#if defined(__APPLE__)
    // find all .app bundles, extract name + version via shell pipeline
    std::string cmd =
        "find /Applications /System/Applications /System/Applications/Utilities"
        " \"$HOME\"/Applications -maxdepth 2 -name '*.app' 2>/dev/null"
        " | head -n " + std::to_string(limit) +
        " | while IFS= read -r app; do"
        " n=$(basename \"$app\" .app);"
        " v=$(plutil -extract CFBundleShortVersionString raw -o - \"$app/Contents/Info.plist\" 2>/dev/null);"
        " printf '%s|%s|%s\\n' \"$n\" \"${v:--}\" \"$app\";"
        " done";

    std::string output = run_command(cmd, 15000);

    if (output.empty()) {
        // fallback: mdfind (Spotlight)
        cmd = "mdfind -count " + std::to_string(limit) +
              " 'kMDItemContentType == \"com.apple.application-bundle\"' 2>/dev/null"
              " | while IFS= read -r app; do"
              " n=$(basename \"$app\" .app);"
              " printf '%s|-|%s\\n' \"$n\" \"$app\";"
              " done";
        output = run_command(cmd, 10000);
    }

    std::istringstream ss(output);
    std::string line;
    while (std::getline(ss, line) && count < limit) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();

        auto p1 = line.find('|');
        auto p2 = line.find('|', p1 + 1);
        if (p1 == std::string::npos || p2 == std::string::npos) continue;

        rows.push_back({
            line.substr(0, p1),
            line.substr(p1 + 1, p2 - p1 - 1),
            "",
            line.substr(p2 + 1)
        });
        ++count;
    }

#elif defined(__linux__)
    // dpkg (Debian / Ubuntu)
    std::string output = run_command(
        "dpkg-query -W -f='${Package}\\t${Version}\\t${Section}\\n' 2>/dev/null | head -n " +
        std::to_string(limit), 12000);

    if (output.empty()) {
        // rpm (RHEL / Fedora / openSUSE)
        output = run_command(
            "rpm -qa --queryformat '%{NAME}\\t%{VERSION}\\t%{GROUP}\\n' 2>/dev/null | head -n " +
            std::to_string(limit), 12000);
    }

    if (output.empty()) {
        // fallback: .desktop files
        std::string desktop_list = run_command(
            "find /usr/share/applications \"$HOME\"/.local/share/applications"
            " /var/lib/snapd/desktop/applications"
            " /var/lib/flatpak/exports/share/applications"
            " -name '*.desktop' 2>/dev/null"
            " | head -n " + std::to_string(limit), 5000);

        std::istringstream fl(desktop_list);
        std::string desktop_path;
        while (std::getline(fl, desktop_path) && count < limit) {
            if (desktop_path.empty()) continue;
            if (desktop_path.back() == '\r') desktop_path.pop_back();

            std::ifstream df(desktop_path);
            if (!df.is_open()) continue;

            std::string ap_name, ap_version, ap_comment;
            std::string dl;
            bool is_app = false;
            while (std::getline(df, dl)) {
                if (!dl.empty() && dl.back() == '\r') dl.pop_back();
                if (dl == "Type=Application") is_app = true;
                if (dl.starts_with("Name=")) ap_name = dl.substr(5);
                if (dl.starts_with("Version=")) ap_version = dl.substr(8);
                if (dl.starts_with("Comment=")) ap_comment = dl.substr(8);
            }
            if (is_app && !ap_name.empty()) {
                rows.push_back({ap_name, ap_version, ap_comment, desktop_path});
                ++count;
            }
        }
    } else {
        std::istringstream ss(output);
        std::string line;
        while (std::getline(ss, line) && count < limit) {
            if (line.empty()) continue;
            if (line.back() == '\r') line.pop_back();

            std::vector<std::string> cols;
            std::istringstream ls(line);
            std::string cell;
            while (std::getline(ls, cell, '\t'))
                cols.push_back(cell);

            if (cols.size() >= 2) {
                rows.push_back({
                    cols[0],
                    cols.size() > 1 ? cols[1] : "",
                    cols.size() > 2 ? cols[2] : "",
                    ""
                });
                ++count;
            }
        }
    }
#endif

    result.rows = std::move(rows);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);
    result.elapsed_ms = elapsed;
    result.ok = true;
    result.summary = std::to_string(result.rows.size()) + " 已安装应用 (用时 " +
                     std::to_string(elapsed.count()) + "ms)";
    return result;
}

} // namespace Nezha::Tools
