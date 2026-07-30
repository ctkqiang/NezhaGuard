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
#include <chrono>
#include <ctime>

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

    constexpr const char *level_to_string(Level lv) noexcept {
        switch (lv) {
            case Level::Trace: return "Trace";
            case Level::Debug: return "Debug";
            case Level::Info: return "Info";
            case Level::Warn: return "Warn";
            case Level::Error: return "Error";
            case Level::Critical: return "Critical";
            default: return "Unknown";
        }
    }

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
                 const char *fmt,
                 Args &&... args) {
            if (!enabled(lv)) return;
            std::string msg = std::vformat(fmt, std::make_format_args(args...));

            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            std::time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm tm = *std::localtime(&tt);
            std::string time_str = std::format(
                "{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec, ms.count()
            );

            std::string full = std::format(
                "[哪吒系统] [{}] @ {}: {}",
                level_to_string(lv), time_str, msg
            );
            write_to_sinks(lv, full);
        }

        void vlogf(Level lv, const char *file, int line, const char *fmt, va_list ap);

    private:
        Logger() = default;

        void write_to_sinks(Level lv, std::string_view line);

        std::atomic<Level> level_{Level::Info};
        mutable std::mutex sink_mtx_;
        std::vector<std::shared_ptr<ISink> > sinks_;
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
#define NZ_LOG_MIN_LEVEL 0
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

#define NZ_CRIT(fmt, ...)  NZ_LOG_IMPL_(::Nezha::Log::Level::Critical, fmt, ##__VA_ARGS__)
