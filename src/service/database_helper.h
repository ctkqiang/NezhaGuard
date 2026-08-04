#pragma once

#ifndef NEZHAGUARD_DATABASE_HELPER_H
#define NEZHAGUARD_DATABASE_HELPER_H

#include <string>
#include <vector>

namespace Nezha::Database {
    enum class DatabaseService : std::uint8_t {
        UNKNOWN = 0,
        SQLITE = 1,
        MYSQL = 2,
        POSTGRES = 3,
        ORACLEDB = 4,
        DB2 = 5
    };

    struct DatabaseInformation {
        std::string Name;
        int Port;
    };

    struct DatabaseConfiguration {
        DatabaseService service = DatabaseService::UNKNOWN;
        std::string DatabaseName;
        std::string DatabaseUser;
        std::string Password;
        std::int32_t DatabasePort = 0;
    };

    struct QuarantineRecord {
        std::string ip_address;
        std::string reason;
        double threat_score;
        std::string quarantined_at;
    };

    class DatabaseHelper {
    public:
        static void InitiateDatabaseService(DatabaseService service);

        static DatabaseInformation GetDefaultInfo(DatabaseService service);

        void Connect();

        static void QuarantineIP(const std::string &ip,
                                 const std::string &reason,
                                 double threat_score);

        static bool IsIPQuarantined(const std::string &ip);

        static void RemoveQuarantine(const std::string &ip);

        [[nodiscard]] static std::vector<QuarantineRecord> GetQuarantineList();

        static void InitializeQuarantineDatabase(const std::string &data_dir = "data/");

    private:
        DatabaseConfiguration m_config;
    };
}

#endif //NEZHAGUARD_DATABASE_HELPER_H
