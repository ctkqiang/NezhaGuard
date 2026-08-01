//
// Created by 钟智强 on 2026/8/1.
//

#ifndef NEZHAGUARD_RULE_LOADER_H
#define NEZHAGUARD_RULE_LOADER_H

#include "detector.h"
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace Nezha::Core {

    enum class RuleLoadError {
        FileNotFound,
        ParseError,
        EmptyRules,
        InvalidType,
        InvalidSeverity,
        InvalidScore,
    };

    const char *rule_load_error_str(RuleLoadError e) noexcept;

    class RuleLoader {
    public:
        RuleLoader() = default;

        // 从 YAML 文件加载规则；失败返回错误码
        [[nodiscard]] std::expected<std::vector<SigRule>, RuleLoadError>
        load_from_file(const std::string &path);

        // 返回编译期内置默认规则（缺文件时 fallback）
        [[nodiscard]] static const std::vector<SigRule> &default_rules();

    private:
        struct RawRule {
            std::string type;
            std::string severity;
            double score = 0.0;
            std::string pattern;
            std::string desc;
        };

        static std::expected<RawRule, RuleLoadError> parse_line(
            std::string_view line, int line_no);
        static std::expected<std::vector<SigRule>, RuleLoadError>
        parse_yaml(const std::string &content);
    };

} // namespace Nezha::Core

#endif //NEZHAGUARD_RULE_LOADER_H
