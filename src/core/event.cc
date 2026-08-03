//
// Created by 钟智强 on 2026/7/30.
//

/**
 * @file    event.cc
 * @brief   Nezha::Core 事件模型（event / EventSource / Severity）的“表示层 / 展示层”实现。
 *
 * 【职责边界（Single Responsibility）】
 * ----------------------------------------------------------------------------
 *   本翻译单元（Translation Unit）只承担一件事：把 event.h 中声明的**数据实体**，
 *   转换为“人类可读的中文文本”。它：
 *     - 不做协议解析、字段校验；
 *     - 不做任何网络 / 文件 I/O；
 *     - 不持有任何可变状态（无全局变量、无静态缓存）；
 *   因此本文件中的三个函数都是**纯函数（pure function）**：给定相同输入，恒返回相同输出，
 *   且无副作用，天然可测试、可并发、可缓存。
 *
 * 【为何将实现从头文件剥离到 .cc（编译期成本与耦合治理）】
 *   1) <sstream> 属于重量级头文件；若将 brief() 的实现放进被广泛 #include 的 event.h，
 *      会把 <sstream> 的解析成本传染给每一个包含 event.h 的 TU，显著拖慢全项目编译。
 *   2) to_cstr / brief 的实现细节对调用方无意义。把实现隐藏在 .cc 中，可缩小 ABI/耦合面，
 *      同时避免潜在的 ODR（One Definition Rule）风险。
 *
 *   - to_cstr 刻意保留 “C 风格 switch + 返回 const char*” 的写法，原因有三：
 *       (a) 零堆分配、返回指向静态只读字符串字面量的指针，生命周期为整个程序期，绝对安全；
 *       (b) 可被 constexpr 化，利于常量折叠；
 *       (c) 不带 default 的穷尽 switch 能让编译器做“枚举穷尽性检查”，配合
 *           -Werror=switch，在未来新增枚举项却漏处理时**直接编译失败**，把运行期缺陷
 *           前移到编译期。
 *   - brief() 目前采用 std::ostringstream，以“可读性优先”。若后续该函数进入热路径，
 *     可平滑迁移到 C++20/23 的 std::format / std::print，利用编译期检查的格式串获得
 *     更好的性能与更严格的类型安全。
 *
 * 【线程安全】
 *   三个函数均为无状态纯函数，读取的是不可变的枚举/入参与只读字符串字面量，
 *   因此在多线程环境下天然可重入，无需任何同步原语。
 */

#include "event.h"
#include <sstream>

namespace Nezha::Core {
    /**
     * @brief   将事件来源枚举 EventSource 映射为中文短名称。
     *
     * @param   s  事件来源（数据包 / 日志 / 蜜罐三路归一化来源之一）。
     * @return  指向**静态只读字符串字面量**的指针；生命周期贯穿整个程序，调用方无需释放。
     *
     * @note    - 标记 noexcept：本函数不会抛出异常，便于被同样 noexcept 的路径调用，
     *            并允许编译器做更激进的优化。
     *          - switch **故意不写 default**：这样编译器可执行枚举穷尽性检查；
     *            当 EventSource 新增枚举项而此处漏加 case 时，能被 -Werror=switch 拦截。
     *          - 末尾的 return "?" 仅用于兜底“非法/越界枚举值”（例如经由整型强转注入的
     *            非法值），是最后一道防御性护栏，正常控制流不应到达。
     */
    const char *to_cstr(const EventSource s) noexcept {
        switch (s) {
            case EventSource::Packet: return "数据包";
            case EventSource::Log: return "日志";
            case EventSource::Honeypot: return "蜜罐";
        }

        return "?";
    }

    /**
     * @brief   将严重级别枚举 Severity 映射为中文短名称。
     *
     * @param   s  严重级别，从低到高：跟踪 < 调试 < 信息 < 警告 < 错误 < 严重。
     * @return  指向静态只读字符串字面量的指针；无需释放，可安全长期持有。
     *
     * @note    设计意图与 to_cstr(EventSource) 完全一致：零分配、noexcept、
     *          无 default 以启用穷尽性检查，末尾 "?" 作为非法枚举值的兜底。
     *          该函数常用于日志/告警渲染，属高频调用点，因此避免任何动态内存分配。
     */
    const char *to_cstr(const Severity s) noexcept {
        switch (s) {
            case Severity::Trace: return "跟踪";
            case Severity::Debug: return "调试";
            case Severity::Warn: return "警告";
            case Severity::Error: return "错误";
            case Severity::Critical: return "严重";
        }

        return "?";
    }

    /**
     * @brief   生成单条事件的“一行摘要（brief）”，用于日志、控制台与告警的人类可读展示。
     *
     * @return  组装完成的中文摘要字符串（按值返回，触发移动/NRVO，无额外拷贝开销）。
     *
     * @details 输出格式约定如下：
     *          @code
     *          [来源] 源IP:源端口 -> 目的IP:目的端口 协议=<num> 字段=<count> [消息="..."]
     *          @endcode
     *          - `to_cstr(source)`：将来源枚举渲染为中文（数据包/日志/蜜罐）。
     *          - `src.to_string()` / `dst.to_string()`：由 ipaddr 统一渲染 v4/v6 文本，
     *            上层无需关心地址族分支（v4 以 ::ffff:a.b.c.d 内部映射存储）。
     *          - `static_cast<unsigned>(proto)`：proto 为 std::uint8_t，若直接 << 会被当作
     *            **字符**输出而非数字，故显式提升为 unsigned 以打印其数值（如 6=TCP）。
     *          - `fields.size()`：仅打印字段数量而非展开全部键值，保证摘要“单行、简短”。
     *          - 仅当 msg 非空时才追加 消息 段，避免在无消息时输出空的 `消息=""` 噪声。
     *
     * @note    - 采用 std::ostringstream 换取实现清晰度与可维护性；其内部会发生一次
     *            动态缓冲分配。若本函数进入性能热路径，建议改用 std::format / std::format_to
     *            并复用输出缓冲区以消除分配。
     *          - 本函数为 const 成员：只读 event 的字段，不修改任何状态，可安全并发调用。
     */
    std::string event::brief() const {
        std::ostringstream os;
        os << '[' << to_cstr(source) << "] "
                << src.to_string() << ':' << sport << " -> "
                << dst.to_string() << ':' << dport
                << " 协议=" << static_cast<unsigned>(proto)
                << " 字段=" << fields.size();

        if (!msg.empty()) os << " 消息=\"" << msg << '"';
        return os.str();
    }
}
