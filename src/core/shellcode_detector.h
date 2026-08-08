//
// Created by 钟智强 on 2026/8/8.
//
// Shellcode pattern detector for binary executables (ELF, Mach-O, PE).
// Scans for known shellcode signatures: NOP sleds, syscall sequences,
// connect-back shellcode, bind shell, and common exploit payload patterns.
//

#ifndef NEZHAGUARD_SHELLCODE_DETECTOR_H
#define NEZHAGUARD_SHELLCODE_DETECTOR_H

#include "types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Nezha::Core {

struct ShellcodeFinding {
    std::string file_path;
    std::string pattern_name;
    std::string description;
    std::uint64_t offset = 0;
    double score = 0.0;
    Severity level = Severity::Info;
    std::string hex_dump; // first 32 bytes of match area
};

class ShellcodeDetector {
public:
    ShellcodeDetector();

    // Add directory or file path to scan.
    void add_target(const std::string &path);

    // Run synchronous scan, returns all detections.
    [[nodiscard]] std::vector<ShellcodeFinding> scan();

    // Scan a single file for shellcode patterns.
    static std::vector<ShellcodeFinding> scan_file(const std::string &path);

    // Check if a file is a scannable binary (ELF, Mach-O, PE, or unknown binary).
    static bool is_binary_file(const std::string &path);

private:
    std::vector<std::string> targets_;

    struct BytePattern {
        const char *name;
        const char *description;
        std::vector<std::uint8_t> bytes; // if non-empty, exact match
        std::vector<std::uint8_t> mask;  // 0xFF = must match, 0x00 = wildcard
        double score;
        Severity level;
    };

    std::vector<BytePattern> patterns_;
    void load_patterns();
};

} // namespace Nezha::Core

#endif // NEZHAGUARD_SHELLCODE_DETECTOR_H
