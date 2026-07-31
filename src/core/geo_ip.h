#ifndef NEZHAGUARD_GEO_IP_H
#define NEZHAGUARD_GEO_IP_H

#include <string>

namespace Nezha::Core {

struct GeoIPResult {
    std::string ip;
    std::string country;
    std::string country_code;
    std::string region;
    std::string city;
    double lat = 0.0;
    double lon = 0.0;
    std::string isp;
    std::string org;
    std::string timezone;
    bool valid = false;
};

class GeoIP {
public:
    GeoIP() = default;

    static GeoIPResult lookup(const std::string &ip);

private:
    static GeoIPResult parse_json(const std::string &json);
    static GeoIPResult lookup_cached(const std::string &ip);
    static void cache_result(const GeoIPResult &r);
};

}

#endif //NEZHAGUARD_GEO_IP_H
