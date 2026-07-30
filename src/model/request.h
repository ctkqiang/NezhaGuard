//
// Created by 钟智强 on 2026/7/31.
//

#ifndef NEZHAGUARD_REQUEST_H
#define NEZHAGUARD_REQUEST_H

#include <string>
#include <optional>
#include <chrono>

namespace Nezha::Data {

    struct Location {
        std::int64_t id;

        double latitude;
        double longitude;

        std::string country_code;
        std::string city;
        std::optional<std::string> region;
    };

    struct UserRequest {
        std::int64_t id;

        std::optional<std::int64_t> location_id;

        std::string ip_address;
        bool is_ipv6{false};

        std::chrono::system_clock::time_point created_at;
    };

}

#endif //NEZHAGUARD_REQUEST_H