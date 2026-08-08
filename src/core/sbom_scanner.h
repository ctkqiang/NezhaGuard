//
// Created by 钟智强 on 2026/8/8.
//
// Sonatype-style C/C++ application vulnerability scanner.
// Finds SBOM (CycloneDX / SPDX) files, extracts CPE identifiers,
// and matches against an embedded database of known critical CVEs.
//

#ifndef NEZHAGUARD_SBOM_SCANNER_H
#define NEZHAGUARD_SBOM_SCANNER_H

#include "types.h"

#include <string>
#include <vector>

namespace Nezha::Core {

struct CpeEntry {
    std::string vendor;
    std::string product;
    std::string version;
};

struct CveRecord {
    std::string cve_id;         // CVE-2024-.....
    std::string cpe_match;      // partial CPE substring to match
    std::string description;    // human-readable
    double cvss_score = 0.0;
    Severity level = Severity::Info;
};

struct SbomFinding {
    std::string file_path;      // SBOM file location
    std::string component;      // affected component name
    std::string version;        // detected version
    std::string cve_id;         // matched CVE
    std::string cpe_raw;        // raw CPE string
    std::string description;    // CVE description
    double cvss_score = 0.0;
    double severity_score = 0.0;
    Severity level = Severity::Info;
};

class SbomScanner {
public:
    SbomScanner();

    // Populate targets (directories to search for SBOM files).
    void add_target(const std::string &path);

    // Run scan synchronously, returns all findings.
    [[nodiscard]] std::vector<SbomFinding> scan();

    // Parse a single CycloneDX JSON SBOM file.
    static std::vector<CpeEntry> parse_cyclonedx(const std::string &path);

    // Parse a single SPDX tag-value or JSON SBOM file.
    static std::vector<CpeEntry> parse_spdx(const std::string &path);

    // Find SBOM files recursively under a directory.
    static std::vector<std::string> find_sbom_files(const std::string &root);

private:
    std::vector<std::string> targets_;
    std::vector<CveRecord> cve_db_;

    void load_cve_database();
    [[nodiscard]] std::vector<SbomFinding> match_cpe(
        const std::string &sbom_path, const std::vector<CpeEntry> &entries) const;
};

} // namespace Nezha::Core

#endif // NEZHAGUARD_SBOM_SCANNER_H