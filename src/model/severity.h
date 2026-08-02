//
// Created by 钟智强 on 2026/7/31.
//
#pragma once

#ifndef NEZHAGUARD_SEVERITY_H
#define NEZHAGUARD_SEVERITY_H

#include <cstdint>
#include <string>

namespace Nezha::Data {

    enum class SeverityLevel : std::uint8_t {
        TRACE = 0,
        DEBUG = 1,
        INFO  = 2,
        WARN  = 3,
        ERROR = 4,
        FATAL = 5
    };

    struct Severity {
        std::int64_t id;

        SeverityLevel level{SeverityLevel::INFO};
        std::uint16_t code{0};

        std::string name;
        std::string description;
    };

}

#endif //NEZHAGUARD_SEVERITY_H