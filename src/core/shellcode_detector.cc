//
// Created by 钟智强 on 2026/8/8.
//

#include "shellcode_detector.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace Nezha::Core {
namespace fs = std::filesystem;

// ── helpers ──
static std::string to_hex(const std::uint8_t *data, size_t len) {
    std::ostringstream ss;
    for (size_t i = 0; i < std::min(len, size_t{32}); ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << ' ';
    return ss.str();
}

static bool match_at(const std::uint8_t *buf, size_t buf_len,
                     const std::vector<std::uint8_t> &bytes,
                     const std::vector<std::uint8_t> &mask, size_t offset) {
    if (offset + bytes.size() > buf_len) return false;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (mask.empty()) {
            if (buf[offset + i] != bytes[i]) return false;
        } else {
            if ((buf[offset + i] & mask[i]) != (bytes[i] & mask[i])) return false;
        }
    }
    return true;
}

// ── embedded shellcode patterns ──
void ShellcodeDetector::load_patterns() {
    // NOP sled variants
    patterns_.push_back({
        "x86_NOP_sled", "x86 NOP sled (0x90 序列 ≥ 16 字节): shellcode 前导特征",
        {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
         0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90},
        {}, 80.0, Severity::Critical});

    patterns_.push_back({
        "x64_NOP_sled", "x86-64 NOP sled (0x90 序列 ≥ 16 字节)",
        {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
         0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90},
        {}, 80.0, Severity::Critical});

    patterns_.push_back({
        "ARM_NOP_sled", "ARM NOP sled (0x00 0x00 0xa0 0xe1 序列)",
        {0x00, 0x00, 0xa0, 0xe1, 0x00, 0x00, 0xa0, 0xe1,
         0x00, 0x00, 0xa0, 0xe1, 0x00, 0x00, 0xa0, 0xe1},
        {}, 75.0, Severity::Critical});

    // x86 syscall sequences
    patterns_.push_back({
        "x86_int80", "x86 int 0x80 syscall (cd 80): linux execve /bin/sh 特征",
        {0xcd, 0x80}, {}, 90.0, Severity::Critical});

    patterns_.push_back({
        "x64_syscall", "x86-64 syscall (0f 05): linux execve /bin/sh 特征",
        {0x0f, 0x05}, {}, 90.0, Severity::Critical});

    // /bin/sh string in shellcode
    patterns_.push_back({
        "binsh_string", "\"/bin/sh\" 字符串: execve shellcode payload",
        {'/', 'b', 'i', 'n', '/', 's', 'h', 0x00},
        {}, 95.0, Severity::Critical});

    patterns_.push_back({
        "sh_string", "\"sh\" 字符串 + null: execve payload",
        {'s', 'h', 0x00}, {}, 85.0, Severity::Critical});

    // Common shellcode prologues
    patterns_.push_back({
        "x86_geteip_fstenv", "x86 fstenv GetEIP 技术: shellcode 自定位",
        {0xd9, 0xee, 0xd9, 0x74, 0x24, 0xf4}, {}, 85.0, Severity::Critical});

    patterns_.push_back({
        "x86_geteip_callpop", "x86 call/pop GetEIP 技术: shellcode 自定位",
        {0xe8, 0x00, 0x00, 0x00, 0x00}, {}, 80.0, Severity::Critical});

    patterns_.push_back({
        "x64_geteip_lea", "x86-64 lea rbx,[rip] GetRIP: shellcode 自定位",
        {0x48, 0x8d, 0x1d}, {0xff, 0xff, 0xff}, 80.0, Severity::Critical});

    // Connect-back shellcode (push socket / connect)
    patterns_.push_back({
        "x86_connectback", "x86 connect-back 框架: push SOCK_STREAM → connect → dup2",
        {0x6a, 0x66, 0x58, 0x99, 0x6a, 0x01, 0x5b, 0x52}, {}, 95.0, Severity::Critical});

    patterns_.push_back({
        "x64_connectback", "x86-64 connect-back 框架: socket → connect → dup2 循环",
        {0x6a, 0x29, 0x58, 0x99, 0x6a, 0x02, 0x5f, 0x6a, 0x01, 0x5e}, {}, 95.0, Severity::Critical});

    // Bind shell
    patterns_.push_back({
        "x86_bindshell", "x86 bind-shell: socket → bind → listen → accept → dup2",
        {0x31, 0xc0, 0x31, 0xdb, 0x31, 0xc9, 0xb0, 0x66}, {}, 95.0, Severity::Critical});

    // Reverse shell strings
    patterns_.push_back({
        "revshell_bash", "bash reverse shell: /dev/tcp/ 字符串",
        {'/', 'd', 'e', 'v', '/', 't', 'c', 'p', '/'}, {}, 92.0, Severity::Critical});

    // Metasploit payload markers
    patterns_.push_back({
        "metasploit_stager", "Metasploit stager: 循环 recv → read → 检查第二个 stage",
        {0xfc, 0xe8, 0x82}, {}, 90.0, Severity::Critical});

    // C2 beacon patterns
    patterns_.push_back({
        "msf_windows_reverse", "msfvenom windows/x64/shell_reverse_tcp 特征",
        {0xfc, 0x48, 0x83, 0xe4, 0xf0, 0xe8, 0xc0, 0x00}, {}, 90.0, Severity::Critical});

    // Egg hunter patterns
    patterns_.push_back({
        "egghunter_sigaction", "Egg hunter (sigaction 法): 内存扫描 shellcode 定位器",
        {0x6a, 0x43, 0x58, 0xcd, 0x80}, {}, 85.0, Severity::Critical});

    // Stack pivot
    patterns_.push_back({
        "stack_pivot_xchg", "Stack pivot: xchg eax,esp → ret (迁移栈到 shellcode)",
        {0x94, 0xc3}, {}, 90.0, Severity::Critical});

    // Heap spray
    patterns_.push_back({
        "heap_spray_0c", "Heap spray 滑板: 0x0c0c0c0c 地址序列",
        {0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
         0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c},
        {}, 75.0, Severity::Error});

    // JMP ESP / CALL ESP (SEH overwrite)
    patterns_.push_back({
        "jmp_esp", "JMP ESP 指令 (ff e4): SEH 覆盖 gadget",
        {0xff, 0xe4}, {}, 85.0, Severity::Critical});

    patterns_.push_back({
        "call_esp", "CALL ESP 指令 (ff d4): SEH 覆盖 gadget",
        {0xff, 0xd4}, {}, 85.0, Severity::Critical});

    // Encoded payload XOR markers
    patterns_.push_back({
        "xor_decoder", "XOR 解码循环: shellcode 自解码器 (fnstenv + xor loop)",
        {0xd9, 0xe1, 0xd9, 0x74, 0x24, 0xf4, 0x58}, {}, 80.0, Severity::Error});

    // Shikata ga nai encoder (metasploit)
    patterns_.push_back({
        "shikata_ga_nai", "Shikata Ga Nai 解码器: FPU fnstenv → XOR 循环",
        {0xd9, 0xee, 0xd9, 0x74, 0x24, 0xf4, 0x5a}, {}, 85.0, Severity::Critical});
}

ShellcodeDetector::ShellcodeDetector() { load_patterns(); }

void ShellcodeDetector::add_target(const std::string &path) {
    targets_.push_back(path);
}

bool ShellcodeDetector::is_binary_file(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    std::array<uint8_t, 4> hdr{};
    f.read(reinterpret_cast<char *>(hdr.data()), 4);
    auto n = f.gcount();
    if (n < 4) return false;

    // ELF magic
    if (hdr[0] == 0x7f && hdr[1] == 'E' && hdr[2] == 'L' && hdr[3] == 'F')
        return true;
    // Mach-O magic (32-bit BE, 32-bit LE, 64-bit LE, 64-bit BE, fat)
    if ((hdr[0] == 0xfe && hdr[1] == 0xed && hdr[2] == 0xfa && (hdr[3] == 0xce || hdr[3] == 0xcf)) ||
        (hdr[0] == 0xce && hdr[1] == 0xfa && hdr[2] == 0xed && hdr[3] == 0xfe) ||
        (hdr[0] == 0xcf && hdr[1] == 0xfa && hdr[2] == 0xed && hdr[3] == 0xfe) ||
        (hdr[0] == 0xca && hdr[1] == 0xfe && hdr[2] == 0xba && hdr[3] == 0xbe))
        return true;
    // PE magic (MZ)
    if (hdr[0] == 'M' && hdr[1] == 'Z')
        return true;
    // Generic binary heuristic: high ratio of non-printable bytes
    // (checked by caller)

    return false;
}

std::vector<ShellcodeFinding> ShellcodeDetector::scan_file(const std::string &path) {
    std::vector<ShellcodeFinding> findings;

    // read file
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return findings;
    size_t size = f.tellg();
    if (size == 0 || size > 100 * 1024 * 1024) return findings; // skip huge files (>100MB)
    f.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> buf(size);
    f.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(size));

    // If not a recognized binary format, do a quick entropy check
    if (!is_binary_file(path)) {
        // count non-printable ratio
        size_t non_printable = 0;
        size_t sample = std::min(size, size_t{4096});
        for (size_t i = 0; i < sample; ++i) {
            auto c = buf[i];
            if (c < 0x09 || (c > 0x0d && c < 0x20)) ++non_printable;
        }
        // if file looks like text, skip binary shellcode scan
        if (non_printable < sample / 10) return findings;
    }

    // ── scan with all patterns ──
    ShellcodeDetector tmp;
    // (patterns are loaded in constructor; we use a static instance)

    static ShellcodeDetector pattern_lib;
    if (pattern_lib.patterns_.empty())
        pattern_lib.load_patterns();

    for (const auto &pat : pattern_lib.patterns_) {
        for (size_t off = 0; off + pat.bytes.size() <= size; ++off) {
            if (match_at(buf.data(), size, pat.bytes, pat.mask, off)) {
                ShellcodeFinding sf;
                sf.file_path = path;
                sf.pattern_name = pat.name;
                sf.description = pat.description;
                sf.offset = off;
                sf.score = pat.score;
                sf.level = pat.level;
                sf.hex_dump = to_hex(&buf[off], std::min(size - off, size_t{32}));
                findings.push_back(sf);

                // skip ahead to avoid duplicate matches on same NOP sled
                off += pat.bytes.size() - 1;
            }
        }
    }

    // deduplicate: keep highest-score finding per pattern per file
    std::ranges::sort(findings, [](const auto &a, const auto &b) {
        return std::tie(a.file_path, a.pattern_name, b.score) <
               std::tie(b.file_path, b.pattern_name, a.score);
    });
    findings.erase(
        std::unique(findings.begin(), findings.end(),
                    [](const auto &a, const auto &b) {
                        return a.file_path == b.file_path && a.pattern_name == b.pattern_name;
                    }),
        findings.end());

    return findings;
}

std::vector<ShellcodeFinding> ShellcodeDetector::scan() {
    std::vector<ShellcodeFinding> all_findings;

    for (const auto &t : targets_) {
        std::error_code ec;
        if (fs::is_regular_file(t, ec)) {
            auto f = scan_file(t);
            all_findings.insert(all_findings.end(), f.begin(), f.end());
        } else if (fs::is_directory(t, ec)) {
            for (auto it = fs::recursive_directory_iterator(
                     t, fs::directory_options::skip_permission_denied, ec);
                 it != fs::recursive_directory_iterator(); ++it) {
                if (it->is_regular_file()) {
                    auto f = scan_file(it->path().string());
                    all_findings.insert(all_findings.end(), f.begin(), f.end());
                }
            }
        }
    }

    std::ranges::sort(all_findings, [](const auto &a, const auto &b) {
        return a.score > b.score;
    });

    return all_findings;
}

} // namespace Nezha::Core