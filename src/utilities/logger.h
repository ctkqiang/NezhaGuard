//
// Created by 钟智强 on 2026/7/30.
//
#pragma once

#include <cstdint>
#include <atomic>
#include <mutex>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <format>
#include <source_location>
#include <cstdarg>

namespace Nezha::Log {

    enum class Level : uint8_t {
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
        Critical,
        Off
    };

    struct ISink {
        virtual ~ISink() = default;
        virtual void write(Level lv, const char *line, std::size_t len) = 0;
        virtual void flush() = 0;
    };

    class Logger {
    public:
        static Logger &instance() noexcept;

        void set_level(Level lv) noexcept {
            level_.store(lv, std::memory_order_relaxed);
        }
        Level level() const noexcept {
            return level_.load(std::memory_order_relaxed);
        }
        bool enabled(Level lv) const noexcept {
            return lv != Level::Off && lv >= level();
        }

        void add_sink(std::shared_ptr<ISink> s);
        void clear_sinks();
        void flush();

        template<typename... Args>

        void log(Level lv,
                 std::format_string<Args...> fmt,
                 Args &&... args,
                 const std::source_location &loc = std::source_location::current())
        {
            if (!enabled(lv)) return;
            std::string msg = std::format(fmt, std::forward<Args>(args)...);
            std::string full = std::format(
                "[{}:{}:{}] {}",
                loc.file_name(), loc.line(), loc.function_name(),
                msg
            );
            write_to_sinks(lv, full);
        }

        void vlogf(Level lv, const char *file, int line, const char *fmt, va_list ap);

    private:
        Logger() = default;
        void write_to_sinks(Level lv, std::string_view line);

        std::atomic<Level> level_{Level::Info};
        mutable std::mutex sink_mtx_;
        std::vector<std::shared_ptr<ISink>> sinks_;
    };

    void init_default(const std::string &file_path, Level lv = Level::Info);

    std::shared_ptr<ISink> make_stderr_sink(bool color = true);

    std::shared_ptr<ISink> make_file_sink(
        const std::string &path,
        std::size_t max_bytes = 32u * 1024 * 1024,
        int max_files = 5
    );

}


#ifndef NZ_LOG_MIN_LEVEL
#define NZ_LOG_MIN_LEVEL 0   // 0=Trace, 1=Debug, 2=Info, 3=Warn, 4=Error, 5=Critical
#endif

#define NZ_LOG_IMPL_(lv, fmt, ...)                                                      \
    do {                                                                                \
        if (::Nezha::Log::Logger::instance().enabled(lv)) {                             \
            ::Nezha::Log::Logger::instance().log(                                       \
                lv, fmt, ##__VA_ARGS__);                                                \
        }                                                                               \
    } while (0)

#if NZ_LOG_MIN_LEVEL <= 0
#define NZ_TRACE(fmt, ...) NZ_LOG_IMPL_(::Nezha::Log::Level::Trace, fmt, ##__VA_ARGS__)
#else
#define NZ_TRACE(fmt, ...) ((void)0)
#endif

#if NZ_LOG_MIN_LEVEL <= 1
#define NZ_DEBUG(fmt, ...) NZ_LOG_IMPL_(::Nezha::Log::Level::Debug, fmt, ##__VA_ARGS__)
#else
#define NZ_DEBUG(fmt, ...) ((void)0)
#endif

#if NZ_LOG_MIN_LEVEL <= 2
#define NZ_INFO(fmt, ...)  NZ_LOG_IMPL_(::Nezha::Log::Level::Info, fmt, ##__VA_ARGS__)
#else
#define NZ_INFO(fmt, ...)  ((void)0)
#endif

#if NZ_LOG_MIN_LEVEL <= 3
#define NZ_WARN(fmt, ...)  NZ_LOG_IMPL_(::Nezha::Log::Level::Warn, fmt, ##__VA_ARGS__)
#else
#define NZ_WARN(fmt, ...)  ((void)0)
#endif

#if NZ_LOG_MIN_LEVEL <= 4
#define NZ_ERROR(fmt, ...) NZ_LOG_IMPL_(::Nezha::Log::Level::Error, fmt, ##__VA_ARGS__)
#else
#define NZ_ERROR(fmt, ...) ((void)0)
#endif

// Critical 永远启用（除非运行时 level 设为 Off）
#define NZ_CRIT(fmt, ...)  NZ_LOG_IMPL_(::Nezha::Log::Level::Critical, fmt, ##__VA_ARGS__)