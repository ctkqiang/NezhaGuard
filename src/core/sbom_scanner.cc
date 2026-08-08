//
// Created by 钟智强 on 2026/8/8.
//

#include "sbom_scanner.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>

namespace Nezha::Core {
namespace fs = std::filesystem;

// ── embedded CVE database (curated critical C/C++ library vulnerabilities) ──
static const CveRecord kCveDb[] = {
    // OpenSSL
    {.cve_id = "CVE-2022-3602", .cpe_match = "openssl:openssl:3.0",
     .description = "OpenSSL 3.0.x 缓冲区溢出 (CVE-2022-3602, CVSS 7.5)", .cvss_score = 7.5, .level = Severity::Critical},
    {.cve_id = "CVE-2022-3786", .cpe_match = "openssl:openssl:3.0",
     .description = "OpenSSL 3.0.x X.509 证书验证溢出 (CVE-2022-3786, CVSS 7.5)", .cvss_score = 7.5, .level = Severity::Critical},
    {.cve_id = "CVE-2023-0286", .cpe_match = "openssl:openssl",
     .description = "OpenSSL X.509 类型混淆导致 RCE (CVE-2023-0286, CVSS 7.4)", .cvss_score = 7.4, .level = Severity::Critical},

    // libcurl
    {.cve_id = "CVE-2023-38545", .cpe_match = "haxx:libcurl",
     .description = "libcurl SOCKS5 堆溢出导致 RCE (CVE-2023-38545, CVSS 9.8)", .cvss_score = 9.8, .level = Severity::Critical},
    {.cve_id = "CVE-2023-38546", .cpe_match = "haxx:libcurl",
     .description = "libcurl cookie 注入 (CVE-2023-38546, CVSS 5.0)", .cvss_score = 5.0, .level = Severity::Error},

    // glibc
    {.cve_id = "CVE-2023-4911", .cpe_match = "gnu:glibc",
     .description = "glibc ld.so Looney Tunables 提权 (CVE-2023-4911, CVSS 7.8)", .cvss_score = 7.8, .level = Severity::Critical},
    {.cve_id = "CVE-2024-2961", .cpe_match = "gnu:glibc",
     .description = "glibc iconv() 缓冲区溢出导致 RCE (CVE-2024-2961, CVSS 8.8)", .cvss_score = 8.8, .level = Severity::Critical},

    // libwebp
    {.cve_id = "CVE-2023-4863", .cpe_match = "webmproject:libwebp",
     .description = "libwebp 堆缓冲区溢出导致 RCE (CVE-2023-4863, CVSS 8.8)", .cvss_score = 8.8, .level = Severity::Critical},

    // zlib / zlib-ng
    {.cve_id = "CVE-2023-45853", .cpe_match = "zlib:zlib",
     .description = "zlib MiniZip 整数溢出 (CVE-2023-45853, CVSS 9.8)", .cvss_score = 9.8, .level = Severity::Critical},

    // libssh2
    {.cve_id = "CVE-2023-48795", .cpe_match = "libssh2:libssh2",
     .description = "libssh2 SSH 协议降级攻击 (Terrapin) (CVE-2023-48795, CVSS 5.9)", .cvss_score = 5.9, .level = Severity::Error},

    // SQLite
    {.cve_id = "CVE-2023-7104", .cpe_match = "sqlite:sqlite",
     .description = "SQLite session 扩展堆缓冲区溢出 (CVE-2023-7104, CVSS 7.5)", .cvss_score = 7.5, .level = Severity::Critical},

    // libxml2
    {.cve_id = "CVE-2024-25062", .cpe_match = "xmlsoft:libxml2",
     .description = "libxml2 实体扩展导致 DoS (CVE-2024-25062, CVSS 7.5)", .cvss_score = 7.5, .level = Severity::Error},

    // Expat
    {.cve_id = "CVE-2024-28757", .cpe_match = "libexpat:expat",
     .description = "Expat XML 解析器整数溢出 (CVE-2024-28757, CVSS 7.5)", .cvss_score = 7.5, .level = Severity::Error},

    // Intel / AMD CPU (applies via kernel/system)
    {.cve_id = "CVE-2023-23583", .cpe_match = "intel",
     .description = "Intel CPU Reptar 瞬态执行 (CVE-2023-23583, CVSS 8.8)", .cvss_score = 8.8, .level = Severity::Critical},
};

// ── known SBOM file patterns ──
static constexpr const char *kSbomPatterns[] = {
    "bom.json", "bom.xml", "cyclonedx.json", "cyclonedx.xml",
    "spdx.json", "spdx.yaml", "spdx.yml",
    "sbom.json", "sbom.xml", "sbom.spdx",
    "manifest.spdx.json", "manifest.cdx.json",
};

// ── common directories to search ──
static constexpr const char *kDefaultSearchDirs[] = {
    "/opt", "/usr/local", "/usr/share", "/var/lib",
    "/Applications", "/Library",
};

SbomScanner::SbomScanner() { load_cve_database(); }

void SbomScanner::load_cve_database() {
    for (const auto &r : kCveDb)
        cve_db_.push_back(r);
}

void SbomScanner::add_target(const std::string &path) {
    targets_.push_back(path);
}

std::vector<std::string> SbomScanner::find_sbom_files(const std::string &root) {
    std::vector<std::string> found;
    std::error_code ec;

    if (!fs::exists(root, ec)) return found;

    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file()) continue;

        auto fname = it->path().filename().string();
        for (const auto *pat : kSbomPatterns) {
            if (fname == pat || fname.ends_with(std::string(".") + pat)) {
                found.push_back(it->path().string());
                break;
            }
        }
    }
    return found;
}

std::vector<CpeEntry> SbomScanner::parse_cyclonedx(const std::string &path) {
    std::vector<CpeEntry> entries;
    std::ifstream f(path);
    if (!f.is_open()) return entries;

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    // extract CPE strings from CycloneDX JSON
    // format: "cpe:2.3:a:vendor:product:version:..."
    size_t pos = 0;
    while ((pos = content.find("\"cpe\"", pos)) != std::string::npos) {
        auto val_start = content.find(':', pos + 5);
        if (val_start == std::string::npos) { ++pos; continue; }
        // skip whitespace and quotes
        val_start = content.find('"', val_start);
        if (val_start == std::string::npos) { ++pos; continue; }
        auto val_end = content.find('"', val_start + 1);
        if (val_end == std::string::npos) { ++pos; continue; }

        std::string cpe = content.substr(val_start + 1, val_end - val_start - 1);

        // parse CPE 2.3: cpe:2.3:a:vendor:product:version:...
        if (cpe.starts_with("cpe:2.3:a:") || cpe.starts_with("cpe:/a:")) {
            CpeEntry entry;
            auto parts_view = cpe;
            // split by ':'
            std::vector<std::string> parts;
            size_t start = 0, end;
            while ((end = parts_view.find(':', start)) != std::string::npos) {
                parts.push_back(parts_view.substr(start, end - start));
                start = end + 1;
            }
            parts.push_back(parts_view.substr(start));

            if (parts.size() >= 7) {
                // cpe:2.3:a:vendor:product:version:...
                entry.vendor = parts[3];
                entry.product = parts[4];
                entry.version = parts[5];
                if (!entry.vendor.empty() && !entry.product.empty())
                    entries.push_back(entry);
            }
        }
        pos = val_end + 1;
    }
    return entries;
}

std::vector<CpeEntry> SbomScanner::parse_spdx(const std::string &path) {
    std::vector<CpeEntry> entries;
    std::ifstream f(path);
    if (!f.is_open()) return entries;

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    // SPDX JSON: "referenceLocator": "cpe:2.3:a:..."
    // SPDX tag-value: ExternalRef: CPE cpe23Type cpe:2.3:a:...
    size_t pos = 0;
    while ((pos = content.find("cpe:2.3:a:", pos)) != std::string::npos) {
        auto end = content.find_first_of("\"\n\r\t ", pos);
        std::string cpe;
        if (end == std::string::npos)
            cpe = content.substr(pos);
        else
            cpe = content.substr(pos, end - pos);

        std::vector<std::string> parts;
        size_t start = 0, p;
        while ((p = cpe.find(':', start)) != std::string::npos) {
            parts.push_back(cpe.substr(start, p - start));
            start = p + 1;
        }
        parts.push_back(cpe.substr(start));

        if (parts.size() >= 7) {
            CpeEntry entry;
            entry.vendor = parts[3];
            entry.product = parts[4];
            entry.version = parts[5];
            if (!entry.vendor.empty() && !entry.product.empty())
                entries.push_back(entry);
        }
        pos = end;
    }
    return entries;
}

std::vector<SbomFinding> SbomScanner::match_cpe(
    const std::string &sbom_path,
    const std::vector<CpeEntry> &entries) const {

    std::vector<SbomFinding> findings;

    for (const auto &entry : entries) {
        std::string lower_vendor = entry.vendor;
        std::string lower_product = entry.product;
        std::ranges::transform(lower_vendor, lower_vendor.begin(),
                               [](char c) { return static_cast<char>(std::tolower(c)); });
        std::ranges::transform(lower_product, lower_product.begin(),
                               [](char c) { return static_cast<char>(std::tolower(c)); });

        for (const auto &cve : cve_db_) {
            std::string match = cve.cpe_match;
            std::ranges::transform(match, match.begin(),
                                   [](char c) { return static_cast<char>(std::tolower(c)); });

            // match on vendor:product substring
            std::string cpe_key = lower_vendor + ":" + lower_product;
            if (cpe_key.find(match) != std::string::npos || match.find(cpe_key) != std::string::npos) {
                findings.push_back({
                    .file_path = sbom_path,
                    .component = entry.vendor + "/" + entry.product,
                    .version = entry.version,
                    .cve_id = cve.cve_id,
                    .cpe_raw = std::format("cpe:2.3:a:{}:{}:{}:*:*:*:*:*:*:*",
                                           entry.vendor, entry.product, entry.version),
                    .description = cve.description,
                    .cvss_score = cve.cvss_score,
                    .severity_score = cve.cvss_score * 10.0,
                    .level = cve.level,
                });
            }
        }
    }

    // deduplicate by CVE + component
    std::ranges::sort(findings, [](const auto &a, const auto &b) {
        return std::tie(a.cve_id, a.component) < std::tie(b.cve_id, b.component);
    });
    auto dup = std::ranges::unique(findings, [](const auto &a, const auto &b) {
        return a.cve_id == b.cve_id && a.component == b.component;
    });
    findings.erase(dup.begin(), dup.end());

    return findings;
}

std::vector<SbomFinding> SbomScanner::scan() {
    std::vector<SbomFinding> all_findings;
    std::vector<std::string> sbom_files;

    if (targets_.empty()) {
        // default: scan common system directories
        for (const auto *dir : kDefaultSearchDirs) {
            auto found = find_sbom_files(dir);
            sbom_files.insert(sbom_files.end(), found.begin(), found.end());
        }
    } else {
        for (const auto &t : targets_) {
            std::error_code ec;
            if (fs::is_regular_file(t, ec)) {
                sbom_files.push_back(t);
            } else if (fs::is_directory(t, ec)) {
                auto found = find_sbom_files(t);
                sbom_files.insert(sbom_files.end(), found.begin(), found.end());
            }
        }
    }

    for (const auto &sf : sbom_files) {
        // try CycloneDX first, then SPDX
        auto entries = parse_cyclonedx(sf);
        if (entries.empty())
            entries = parse_spdx(sf);

        if (!entries.empty()) {
            auto findings = match_cpe(sf, entries);
            all_findings.insert(all_findings.end(), findings.begin(), findings.end());
        }
    }

    // sort by CVSS score descending
    std::ranges::sort(all_findings, [](const auto &a, const auto &b) {
        return a.cvss_score > b.cvss_score;
    });

    return all_findings;
}

} // namespace Nezha::Core