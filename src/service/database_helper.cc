//
// Created by 钟智强 on 2026/7/31.
//

/**
 * @file    database_helper.cc
 * @brief   Nezha::Database::DatabaseHelper 的实现单元。
 *
 * ============================================================================
 * 【职责边界】
 * ----------------------------------------------------------------------------
 *   本翻译单元实现 database_helper.h 中声明的门面方法。当前仅承担两件事：
 *     1) GetDefaultInfo：把 DatabaseService 枚举映射为其默认元信息（纯查询）；
 *     2) InitiateDatabaseService：服务初始化入口（当前为占位，待接入真实连接层）。
 *   本文件不持有全局可变状态，GetDefaultInfo 为无副作用纯函数，可安全并发调用。
 *
 * 【C++26 视角的取舍说明】
 *   - GetDefaultInfo 采用 switch 逐一枚举，并借助聚合初始化 `{"名称", 端口}`
 *     直接构造返回值（NRVO/移动），零冗余拷贝、可读性高。
 *   - 这里保留了 `default` 分支：与 UNKNOWN 合并统一返回哨兵值 {"Unknown", -1}，
 *     用于兜底“经整型强转注入的非法枚举值”，提供防御性护栏。
 *     （注：若希望编译器强制未来新增枚举项的穷尽处理，可移除 default 并配合
 *      -Werror=switch；本文件出于对非法数值的运行期防御选择保留 default。）
 */

#include "database_helper.h"

#include <iostream>

namespace Nezha::Database {
    /**
     * @brief   初始化指定数据库服务：解析其默认元信息并输出到标准输出。
     *
     * @param   service  目标数据库种类（以 const 传值，函数体内不可修改，语义清晰）。
     *
     * @details 执行流程：
     *          1) 调用 GetDefaultInfo(service) 取回该数据库的默认元信息
     *             （展示名 Name + 默认端口 Port）；
     *          2) 使用 C++17 结构化绑定 `auto [Name, Port]` 将返回的
     *             DatabaseInformation 直接解构为两个具名局部变量，避免
     *             `info.Name` / `info.Port` 的冗余成员访问，提升可读性；
     *          3) 将“正在初始化”的提示信息打印到 std::cout。
     *
     * @note    - 结构化绑定以 const 限定，表明这两个局部量为只读快照，
     *            不会被后续逻辑意外修改。
     *          - 当前实现仅做“信息展示”占位：真实的连接建立、鉴权与连接池
     *            初始化逻辑将在后续版本补充；届时应结合 DatabaseConfiguration
     *            用 Port 作为默认端口回填并发起实际连接。
     *          - 若目标为未知/非法枚举值，GetDefaultInfo 会返回
     *            {"Unknown", -1}，此处会原样打印该哨兵值，便于问题定位。
     */
    void DatabaseHelper::InitiateDatabaseService(const DatabaseService service) {
        const auto [Name, Port] = GetDefaultInfo(service);

        std::cout << "正在初始化数据库服务：" << Name << "，默认端口：" << Port << '\n';
    }

    /**
     * @brief   将数据库种类枚举映射为其默认元信息（展示名 + 默认端口）。
     *
     * @param   service  目标数据库种类。
     * @return  对应的 DatabaseInformation。约定：
     *            - SQLite 无网络端口，端口记为 0；
     *            - UNKNOWN 或任何非法/越界枚举值统一回退为 {"Unknown", -1}。
     *
     * @note    - 使用聚合初始化直接构造返回对象，避免临时变量与多余拷贝。
     *          - 端口取值来源于各数据库的 IANA 默认端口：
     *              MySQL=3306、PostgreSQL=5432、Oracle=1521、DB2=50000。
     */
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
