//
// Created by 钟智强 on 2026/8/4.
//

#include "database_helper.h"
#include "../contants.h"

#include <iostream>
#include <mutex>
#include <sqlite3.h>
#include <unordered_set>

namespace Nezha::Database {
    namespace {
        class DbGuard {
        public:
            DbGuard() = default;

            auto open(std::string_view path) -> bool {
                if (db_) return true;
                if (sqlite3_open(path.data(), &db_) != SQLITE_OK) {
                    std::cerr << std::format("[DB] 打开失败: {}\n", sqlite3_errmsg(db_));
                    return false;
                }
                sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
                sqlite3_exec(db_, "PRAGMA busy_timeout=3000;", nullptr, nullptr, nullptr);
                exec(QuarantineRecord::ddl);
                return true;
            }

            auto handle() -> sqlite3 * { return db_; }
            operator bool() const { return db_ != nullptr; }

            auto exec(std::string_view sql) -> bool {
                char *err = nullptr;
                if (sqlite3_exec(db_, sql.data(), nullptr, nullptr, &err) != SQLITE_OK) {
                    std::cerr << std::format("[DB] exec 失败: {}\n", err);
                    sqlite3_free(err);
                    return false;
                }
                return true;
            }

            ~DbGuard() { if (db_) sqlite3_close(db_); }

            DbGuard(const DbGuard &) = delete;

            DbGuard &operator=(const DbGuard &) = delete;

        private:
            sqlite3 *db_ = nullptr;
        };

        class Stmt {
        public:
            Stmt(sqlite3 *db, std::string_view sql) {
                sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &stmt_, nullptr);
            }

            ~Stmt() { if (stmt_) sqlite3_finalize(stmt_); }

            Stmt(const Stmt &) = delete;

            Stmt &operator=(const Stmt &) = delete;

            operator bool() const { return stmt_ != nullptr; }

            template<typename... Args>
            auto bind(Args &&... args) -> bool {
                int idx = 1;
                return (bind_one(idx++, std::forward<Args>(args)) && ...);
            }

            auto step() const -> int { return sqlite3_step(stmt_); }

            auto col_text(int i) const -> const char * {
                return reinterpret_cast<const char *>(sqlite3_column_text(stmt_, i));
            }

            auto col_double(const int i) const -> double { return sqlite3_column_double(stmt_, i); }
            auto col_int64(int i) const -> int64_t { return sqlite3_column_int64(stmt_, i); }

        private:
            sqlite3_stmt *stmt_ = nullptr;

            auto bind_one(const int idx, std::string_view v) const -> bool {
                return sqlite3_bind_text(stmt_, idx, v.data(),
                                         static_cast<int>(v.size()), SQLITE_TRANSIENT) == SQLITE_OK;
            }

            auto bind_one(const int idx, const double v) const -> bool {
                return sqlite3_bind_double(stmt_, idx, v) == SQLITE_OK;
            }

            auto bind_one(const int idx, const int64_t v) const -> bool {
                return sqlite3_bind_int64(stmt_, idx, v) == SQLITE_OK;
            }
        };

        class QuarantineRepo {
        public:
            auto init(std::string_view data_dir) -> void {
                std::lock_guard lock(mtx_);
                db_path_ = std::format("{}/nezha_quarantine.db", data_dir);
                if (!db_.open(db_path_)) return;
                load_cache();
            }

            auto insert(std::string_view ip, std::string_view reason, double score) -> void {
                std::lock_guard lock(mtx_);
                ensure_open();

                Stmt stmt(db_.handle(), Orm<QuarantineRecord>::insert_or_replace_sql());
                if (!stmt || !stmt.bind(ip, reason, score)) {
                    std::cerr << "[DB] bind insert 失败\n";
                    return;
                }
                if (stmt.step() != SQLITE_DONE)
                    std::cerr << std::format("[DB] insert: {}\n", sqlite3_errmsg(db_.handle()));
                else {
                    cache_.emplace(ip);
                    std::cout << std::format("[DB] 已隔离: {}  原因: {}\n", ip, reason);
                }
            }

            auto contains(std::string_view ip) -> bool {
                std::lock_guard lock(mtx_);
                return cache_.contains(std::string(ip));
            }

            auto remove(std::string_view ip) -> void {
                std::lock_guard lock(mtx_);
                cache_.erase(std::string(ip));
                ensure_open();
                Stmt stmt(db_.handle(), Orm<QuarantineRecord>::delete_where("ip_address"));
                if (stmt && stmt.bind(ip)) stmt.step();
            }

            auto fetch_all() -> std::vector<QuarantineRecord> {
                std::lock_guard lock(mtx_);
                ensure_open();
                std::vector<QuarantineRecord> list;

                Stmt stmt(db_.handle(), Orm<QuarantineRecord>::all()
                          .order_by_desc("id")
                          .sql());
                if (!stmt) return list;

                while (stmt.step() == SQLITE_ROW) {
                    QuarantineRecord r{};
                    r.id = stmt.col_int64(0);
                    r.ip_address = stmt.col_text(1);
                    r.reason = stmt.col_text(2);
                    r.threat_score = stmt.col_double(3);
                    r.quarantined_at = stmt.col_text(4) ? stmt.col_text(4) : "";
                    list.push_back(std::move(r));
                }
                return list;
            }

        private:
            DbGuard db_;
            std::string db_path_;
            std::unordered_set<std::string> cache_;
            std::mutex mtx_;

            auto ensure_open() -> void { db_.open(db_path_); }

            auto load_cache() -> void {
                Stmt stmt(db_.handle(), "SELECT ip_address FROM quarantine");
                if (!stmt) return;
                while (stmt.step() == SQLITE_ROW)
                    cache_.emplace(stmt.col_text(0));
            }
        };

        QuarantineRepo g_repo; // 全局单例
    }


    void DatabaseHelper::InitializeQuarantineDatabase(const std::string &data_dir) {
        g_repo.init(data_dir);
    }

    void DatabaseHelper::QuarantineIP(std::string_view ip,
                                      std::string_view reason,
                                      double threat_score) {
        g_repo.insert(ip, reason, threat_score);
    }

    bool DatabaseHelper::IsIPQuarantined(std::string_view ip) {
        return g_repo.contains(ip);
    }

    void DatabaseHelper::RemoveQuarantine(std::string_view ip) {
        g_repo.remove(ip);
    }

    std::vector<QuarantineRecord> DatabaseHelper::GetQuarantineList() {
        return g_repo.fetch_all();
    }

    void DatabaseHelper::InitiateDatabaseService(const DatabaseService service) {
        auto [Name, Port] = GetDefaultInfo(service);
        std::cout << std::format("初始化数据库服务: {} 端口={}\n", Name, Port);
    }

    DatabaseInformation DatabaseHelper::GetDefaultInfo(DatabaseService service) {
        switch (service) {
            case DatabaseService::MYSQL: return {.Name = "MySQL", .Port = 3306};
            case DatabaseService::POSTGRES: return {.Name = "PostgreSQL", .Port = 5432};
            case DatabaseService::SQLITE: return {.Name = "SQLite", .Port = 0};
            case DatabaseService::ORACLEDB: return {.Name = "Oracle DB", .Port = 1521};
            case DatabaseService::DB2: return {.Name = "IBM DB2", .Port = 50000};
            default: return {.Name = "Unknown", .Port = -1};
        }
    }
}
