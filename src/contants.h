//
// Created by 钟智强 on 2026/7/31.
//
#pragma once

#ifndef NEZHAGUARD_CONTANTS_H
#define NEZHAGUARD_CONTANTS_H

namespace Nezha::Configuration {
    class ApplicationConstants {
    public:
        ApplicationConstants() = delete;

        static constexpr const char *ApplicationVersion = "v0.0.1";
        static constexpr bool ShowGui = true;
        static constexpr bool ShowOtherApplicationLogs = true;

        static constexpr int64_t AnomaliesQuarantineThreshold = 12;
    };
}

#endif //NEZHAGUARD_CONTANTS_H