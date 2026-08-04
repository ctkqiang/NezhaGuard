#include "database_helper.h"
#include "../contants.h"

#include <iostream>
#include <mutex>
#include <sqlite3.h>
#include <unordered_set>

namespace Nezha::Database {
    namespace {
        sqlite3 *g_quarantine_db = nullptr;
        std::mutex g_db_mutex;
        std::unordered_set<std::string> g_quarantine_cache;

        constexpr const char *kQuarantineDBPath = "nezha_quarantine.db";

        constexpr const char *kCreateTableSQL = R"SQL(
            CREATE TABLE IF NOT EXISTS quarantine (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                ip_address  TEXT    NOT NULL UNIQUE,
                reason      TEXT    NOT NULL,
                threat_score REAL   DEFAULT 0,
                created_at  TEXT   DEFAULT (datetime('now','localtime'))
            );
        )SQL";

        constexpr const char *kInsertSQL =
                "INSERT OR REPLACE INTO quarantine (ip_address, reason, threat_score) "
                "VALUES (?, ?, ?);";

        constexpr const char *kSelectOneSQL =
                "SELECT COUNT(*) FROM quarantine WHERE ip_address = ?;";

        constexpr const char *kDeleteSQL =
                "DELETE FROM quarantine WHERE ip_address = ?;";

        constexpr const char *kSelectAllSQL =
                "SELECT ip_address, reason, threat_score, created_at FROM quarantine "
                "ORDER BY created_at DESC;";

        void EnsureDB() {
            if (g_quarantine_db) return;

            if (sqlite3_open(kQuarantineDBPath, &g_quarantine_db) != SQLITE_OK) {
                std::cerr << "[DatabaseHelper] 无法打开隔离数据库: "
                        << sqlite3_errmsg(g_quarantine_db) << '\n';
                return;
            }

            sqlite3_exec(g_quarantine_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
            char *err = nullptr;
            if (sqlite3_exec(g_quarantine_db, kCreateTableSQL, nullptr, nullptr, &err) != SQLITE_OK) {
                std::cerr << "[DatabaseHelper] 创建隔离表失败: " << err << '\n';
                sqlite3_free(err);
            }

            const char *loadSQL = "SELECT ip_address FROM quarantine;";

            sqlite3_stmt *stmt = nullptr;
            if (sqlite3_prepare_v2(g_quarantine_db, loadSQL, -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    g_quarantine_cache.insert(
                        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
                }
                sqlite3_finalize(stmt);
            }
        }
    }

    void DatabaseHelper::InitializeQuarantineDatabase() {
        std::lock_guard<std::mutex> lock(g_db_mutex);
        EnsureDB();
    }

    void DatabaseHelper::QuarantineIP(const std::string &ip,
                                      const std::string &reason,
                                      double threat_score) {
        std::lock_guard<std::mutex> lock(g_db_mutex);
        EnsureDB();
        if (!g_quarantine_db) return;

        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(g_quarantine_db, kInsertSQL, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "[DatabaseHelper] prepare insert: "
                    << sqlite3_errmsg(g_quarantine_db) << '\n';
            return;
        }

        sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, reason.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 3, threat_score);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "[DatabaseHelper] 隔离IP写入失败: "
                    << sqlite3_errmsg(g_quarantine_db) << '\n';
        } else {
            g_quarantine_cache.insert(ip);
            std::cout << "[DatabaseHelper] IP已隔离: " << ip
                    << " 原因: " << reason << '\n';
        }

        sqlite3_finalize(stmt);
    }

    bool DatabaseHelper::IsIPQuarantined(const std::string &ip) {
        {
            std::lock_guard<std::mutex> lock(g_db_mutex);
            if (g_quarantine_cache.count(ip)) return true;
        }
        return false;
    }

    void DatabaseHelper::RemoveQuarantine(const std::string &ip) {
        std::lock_guard<std::mutex> lock(g_db_mutex);
        g_quarantine_cache.erase(ip);
        EnsureDB();
        if (!g_quarantine_db) return;

        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(g_quarantine_db, kDeleteSQL, -1, &stmt, nullptr) != SQLITE_OK) { return; }

        sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    std::vector<QuarantineRecord> DatabaseHelper::GetQuarantineList() {
        std::lock_guard<std::mutex> lock(g_db_mutex);
        EnsureDB();
        std::vector<QuarantineRecord> list;
        if (!g_quarantine_db) return list;

        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(g_quarantine_db, kSelectAllSQL, -1, &stmt, nullptr) != SQLITE_OK) {
            return list;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            QuarantineRecord r{};
            r.ip_address = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            r.reason = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            r.threat_score = sqlite3_column_double(stmt, 2);
            r.quarantined_at = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
            list.push_back(std::move(r));
        }
        sqlite3_finalize(stmt);
        return list;
    }

    void DatabaseHelper::InitiateDatabaseService(const DatabaseService service) {
        const auto [Name, Port] = GetDefaultInfo(service);
        std::cout << "正在初始化数据库服务：" << Name << "，默认端口：" << Port << '\n';
    }

    DatabaseInformation DatabaseHelper::GetDefaultInfo(DatabaseService service) {
        switch (service) {
            case DatabaseService::MYSQL:
                return {.Name = "MySQL", .Port = 3306};
            case DatabaseService::POSTGRES:
                return {.Name = "PostgreSQL", .Port = 5432};
            case DatabaseService::SQLITE:
                return {.Name = "SQLite", .Port = 0};
            case DatabaseService::ORACLEDB:
                return {.Name = "Oracle DB", .Port = 1521};
            case DatabaseService::DB2:
                return {.Name = "IBM DB2", .Port = 50000};
            case DatabaseService::UNKNOWN:
            default:
                return {.Name = "Unknown", .Port = -1};
        }
    }
}