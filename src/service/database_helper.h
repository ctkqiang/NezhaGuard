//
// Created by 钟智强 on 2026/7/31.
//

#ifndef NEZHAGUARD_DATABASE_HELPER_H
#define NEZHAGUARD_DATABASE_HELPER_H

#include <string>

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

    class DatabaseHelper {
    public:
        static void InitiateDatabaseService(DatabaseService service);

        static DatabaseInformation GetDefaultInfo(DatabaseService service);

        void Connect();

    private:
        DatabaseConfiguration m_config;
    };
}

#endif // NEZHAGUARD_DATABASE_HELPER_H
