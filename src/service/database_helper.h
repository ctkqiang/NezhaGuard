//
// Created by 钟智强 on 2026/8/4.
//

#pragma once

#ifndef NEZHAGUARD_DATABASE_HELPER_H
#define NEZHAGUARD_DATABASE_HELPER_H

#include <chrono>
#include <concepts>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace Nezha::Database {

enum class SqlType { Integer, Real, Text, Blob };

template <typename T> constexpr SqlType sql_type_of();
template <> constexpr SqlType sql_type_of<int64_t>()     { return SqlType::Integer; }
template <> constexpr SqlType sql_type_of<double>()      { return SqlType::Real; }
template <> constexpr SqlType sql_type_of<std::string>() { return SqlType::Text; }

template <typename T>
concept Ormable = requires {
    { T::table_name } -> std::convertible_to<std::string_view>;
    typename T::key_type;
    // columns 是 std::tuple<Column...>, 编译期只检查存在性
    requires (std::tuple_size_v<decltype(T::columns)> > 0);
};


template <typename Record, typename FieldType>
struct Column {
    std::string_view   name;        // SQL 列名
    FieldType Record::*member;      // 结构体成员指针
    bool               insertable = true;  // false = 自动生成列(created_at 等)

    constexpr auto sql_def() const -> std::string {
        if constexpr (std::same_as<FieldType, std::string>)
            return std::format("{} TEXT NOT NULL", name);
        else if constexpr (std::same_as<FieldType, double>)
            return std::format("{} REAL DEFAULT 0", name);
        else if constexpr (std::same_as<FieldType, int64_t>)
            return std::format("{} INTEGER", name);
        else
            return std::format("{} TEXT", name);
    }
};

struct QuarantineRecord {
    int64_t     id           = 0;
    std::string ip_address;
    std::string reason;
    double      threat_score = 0.0;
    std::string quarantined_at;      // 映射到 SQL 的 created_at 列

    using key_type = std::string;

    static constexpr std::string_view table_name = "quarantine";

    static constexpr auto columns = std::make_tuple(
        Column<QuarantineRecord, std::string>{"ip_address",    &QuarantineRecord::ip_address},
        Column<QuarantineRecord, std::string>{"reason",        &QuarantineRecord::reason},
        Column<QuarantineRecord, double>     {"threat_score",  &QuarantineRecord::threat_score},
        Column<QuarantineRecord, std::string>{"created_at",    &QuarantineRecord::quarantined_at, false}
    );

    static constexpr std::string_view ddl = R"SQL(
        CREATE TABLE IF NOT EXISTS quarantine (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            ip_address   TEXT    NOT NULL UNIQUE,
            reason       TEXT    NOT NULL,
            threat_score REAL   DEFAULT 0,
            created_at   TEXT   DEFAULT (datetime('now','localtime'))
        );
    )SQL";
};

enum class DatabaseService : std::uint8_t { UNKNOWN, SQLITE, MYSQL, POSTGRES, ORACLEDB, DB2 };

struct DatabaseInformation { std::string Name; int Port; };
struct DatabaseConfiguration {
    DatabaseService service = DatabaseService::UNKNOWN;
    std::string DatabaseName, DatabaseUser, Password;
    std::int32_t DatabasePort = 0;
};

template <Ormable Record>
class Orm {
public:
    using RowCallback = std::function<void(Record &)>;

    Orm() = default;

    static auto all() -> Orm {
        Orm o;
        o.sql_ = std::format("SELECT id,{} FROM {}",
                             join_select_cols(), Record::table_name);
        return o;
    }

    static auto where_eq(std::string_view col) -> Orm {
        Orm o;
        o.sql_ = std::format("SELECT id,{} FROM {} WHERE {}=?",
                             join_select_cols(), Record::table_name, col);
        return o;
    }

    static auto count_where(std::string_view col) -> Orm {
        Orm o;
        o.sql_ = std::format("SELECT COUNT(*) FROM {} WHERE {}=?",
                             Record::table_name, col);
        return o;
    }

    static auto insert_or_replace_sql() -> std::string {
        size_t n = count_insertable();
        return std::format("INSERT OR REPLACE INTO {} ({}) VALUES ({})",
                           Record::table_name, join_insert_cols(), placeholders(n));
    }

    static auto delete_where(std::string_view col) -> std::string {
        return std::format("DELETE FROM {} WHERE {}=?", Record::table_name, col);
    }

    static constexpr auto column_count() -> size_t {
        return std::tuple_size_v<decltype(Record::columns)>;
    }

    auto order_by_desc(std::string_view col) -> Orm& {
        if (const auto pos = sql_.find("ORDER BY"); pos == std::string::npos) {
            sql_ += std::format(" ORDER BY {} DESC", col);
        }
        return *this;
    }

    [[nodiscard]] auto sql() const -> std::string_view { return sql_; }

private:
    std::string sql_;

    static auto join_select_cols() -> std::string {
        return fold_all([](auto &c) -> std::string_view { return c.name; }, ", ");
    }

    static auto join_insert_cols() -> std::string {
        return fold_insertable([](auto &c) -> std::string_view { return c.name; }, ", ");
    }

    static constexpr auto count_insertable() -> size_t {
        return [&]<size_t... I>(std::index_sequence<I...>) {
            return ((std::get<I>(Record::columns).insertable ? 1 : 0) + ...);
        }(std::make_index_sequence<column_count()>{});
    }

    static auto placeholders(const size_t n) -> std::string {
        std::string r;
        for (size_t i = 0; i < n; ++i) {
            if (i) r += ", ";
            r += '?';
        }
        return r;
    }

    template <typename Fn>
    static auto fold_all(Fn &&fn, const std::string_view sep) -> std::string {
        return [&]<size_t... I>(std::index_sequence<I...>) {
            std::string r;
            auto add = [&](std::string_view s) {
                if (!r.empty()) r += sep;
                r += s;
            };
            (add(fn(std::get<I>(Record::columns))), ...);
            return r;
        }(std::make_index_sequence<column_count()>{});
    }

    template <typename Fn>
    static auto fold_insertable(Fn &&fn, const std::string_view sep) -> std::string {
        return [&]<size_t... I>(std::index_sequence<I...>) {
            std::string r;
            auto add = [&]<typename Col>(Col &c) {
                if (!c.insertable) return;
                if (!r.empty()) r += sep;
                r += fn(c);
            };
            (add(std::get<I>(Record::columns)), ...);
            return r;
        }(std::make_index_sequence<column_count()>{});
    }
};

struct HoneypotSessionRecord {
    int64_t     id = 0;
    std::string attacker_ip;
    std::string attacker_port;
    std::string service;           // SSH, MySQL, HTTP...
    std::string payload;           // attacker input / command
    std::string technique;         // MITRE ATT&CK tag
    std::string geo_country;
    std::string geo_city;
    std::string geo_isp;
    double      threat_score = 0.0;
    std::string captured_at;

    using key_type = std::string;
    static constexpr std::string_view table_name = "honeypot_sessions";

    static constexpr auto columns = std::make_tuple(
        Column<HoneypotSessionRecord, std::string>{"attacker_ip",   &HoneypotSessionRecord::attacker_ip},
        Column<HoneypotSessionRecord, std::string>{"attacker_port", &HoneypotSessionRecord::attacker_port},
        Column<HoneypotSessionRecord, std::string>{"service",       &HoneypotSessionRecord::service},
        Column<HoneypotSessionRecord, std::string>{"payload",       &HoneypotSessionRecord::payload},
        Column<HoneypotSessionRecord, std::string>{"technique",     &HoneypotSessionRecord::technique},
        Column<HoneypotSessionRecord, std::string>{"geo_country",   &HoneypotSessionRecord::geo_country},
        Column<HoneypotSessionRecord, std::string>{"geo_city",      &HoneypotSessionRecord::geo_city},
        Column<HoneypotSessionRecord, std::string>{"geo_isp",       &HoneypotSessionRecord::geo_isp},
        Column<HoneypotSessionRecord, double>     {"threat_score",  &HoneypotSessionRecord::threat_score},
        Column<HoneypotSessionRecord, std::string>{"captured_at",   &HoneypotSessionRecord::captured_at, false}
    );

    static constexpr std::string_view ddl = R"SQL(
        CREATE TABLE IF NOT EXISTS honeypot_sessions (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            attacker_ip   TEXT NOT NULL,
            attacker_port TEXT DEFAULT '',
            service       TEXT DEFAULT '',
            payload       TEXT DEFAULT '',
            technique     TEXT DEFAULT '',
            geo_country   TEXT DEFAULT '',
            geo_city      TEXT DEFAULT '',
            geo_isp       TEXT DEFAULT '',
            threat_score  REAL DEFAULT 0,
            captured_at   TEXT DEFAULT (datetime('now','localtime'))
        );
    )SQL";
};

class DatabaseHelper {
public:
    static auto InitializeQuarantineDatabase(const std::string &data_dir = "data/") -> void;
    static auto QuarantineIP(std::string_view ip, std::string_view reason, double threat_score) -> void;
    static auto IsIPQuarantined(std::string_view ip) -> bool;
    static auto RemoveQuarantine(std::string_view ip) -> void;
    [[nodiscard]] static auto GetQuarantineList() -> std::vector<QuarantineRecord>;

    // honeypot sessions
    static auto InsertHoneypotSession(const HoneypotSessionRecord &rec) -> void;
    [[nodiscard]] static auto GetHoneypotSessions(int limit = 200) -> std::vector<HoneypotSessionRecord>;

    static void InitiateDatabaseService(DatabaseService service);
    static auto GetDefaultInfo(DatabaseService service) -> DatabaseInformation;
};

}

#endif