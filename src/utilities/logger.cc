//
// Created by 钟智强 on 2026/7/30.
//
#include "logger.h"

#include <cstdio>
#include <cstdarg>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <ctime>

namespace fs = std::filesystem;

namespace Nezha::Log {
    namespace {
        class StderrSink final : public ISink {
        public:
            explicit StderrSink(bool use_color) : color_(use_color) {
            }

            void write(Level lv, const char *line, std::size_t len) override {
                if (color_) {
                    const char *color_code = "";
                    switch (lv) {
                        case Level::Trace: color_code = "\033[37m";
                            break;
                        case Level::Debug: color_code = "\033[36m";
                            break;
                        case Level::Info: color_code = "\033[32m";
                            break;
                        case Level::Warn: color_code = "\033[33m";
                            break;
                        case Level::Error: color_code = "\033[31m";
                            break;
                        case Level::Critical: color_code = "\033[41;37m";
                            break;
                        default: break;
                    }
                    std::fprintf(stderr, "%s%.*s\033[0m\n", color_code, static_cast<int>(len), line);
                } else {
                    std::fprintf(stderr, "%.*s\n", static_cast<int>(len), line);
                }
            }

            void flush() override {
                std::fflush(stderr);
            }

        private:
            bool color_;
        };
    }

    namespace {
        class FileSink final : public ISink {
        public:
            FileSink(const std::string &path, std::size_t max_bytes, int max_files)
                : path_(path), max_bytes_(max_bytes), max_files_(max_files) {
                fs::create_directories(fs::path(path_).parent_path());
                if (fs::exists(path_)) {
                    current_size_ = fs::file_size(path_);
                }
                open_file();
            }

            ~FileSink() override {
                if (file_.is_open()) {
                    file_.close();
                }
            }

            void write(Level /*lv*/, const char *line, std::size_t len) override {
                if (!file_.is_open()) {
                    open_file();
                }
                if (current_size_ + len + 1 > max_bytes_) {
                    rotate();
                    open_file();
                    current_size_ = 0;
                }
                file_.write(line, static_cast<std::streamsize>(len));
                file_.put('\n');
                file_.flush();
                current_size_ += len + 1;
            }

            void flush() override {
                if (file_.is_open()) {
                    file_.flush();
                }
            }

        private:
            void open_file() {
                file_.open(path_, std::ios::app | std::ios::out);
                if (!file_.is_open()) {
                    std::fprintf(stderr, "[日志器] 打开日志文件失败: %s\n", path_.c_str());
                }
            }

            void rotate() {
                file_.close();
                for (int i = max_files_ - 1; i >= 1; --i) {
                    std::string old_name = path_ + "." + std::to_string(i);
                    std::string new_name = path_ + "." + std::to_string(i + 1);
                    if (fs::exists(old_name)) {
                        if (fs::exists(new_name)) {
                            fs::remove(new_name);
                        }
                        fs::rename(old_name, new_name);
                    }
                }
                std::string first = path_ + ".1";
                if (fs::exists(first)) {
                    fs::remove(first);
                }
                if (fs::exists(path_)) {
                    fs::rename(path_, first);
                }
            }

            std::string path_;
            std::size_t max_bytes_;
            int max_files_;
            std::size_t current_size_ = 0;
            std::ofstream file_;
        };
    }

    Logger &Logger::instance() noexcept {
        static Logger instance;
        return instance;
    }

    void Logger::add_sink(std::shared_ptr<ISink> s) {
        if (!s) return;
        std::lock_guard<std::mutex> lock(sink_mtx_);
        sinks_.push_back(std::move(s));
    }

    void Logger::clear_sinks() {
        std::lock_guard<std::mutex> lock(sink_mtx_);
        sinks_.clear();
    }

    void Logger::flush() {
        std::lock_guard<std::mutex> lock(sink_mtx_);
        for (auto &s: sinks_) {
            s->flush();
        }
    }

    void Logger::write_to_sinks(Level lv, std::string_view line) {
        std::lock_guard<std::mutex> lock(sink_mtx_);
        for (auto &s: sinks_) {
            s->write(lv, line.data(), line.size());
        }
    }

    void Logger::vlogf(Level lv, const char * /*file*/, int /*line*/, const char *fmt, va_list ap) {
        if (!enabled(lv)) return;
        va_list ap_copy;
        va_copy(ap_copy, ap);
        int len = std::vsnprintf(nullptr, 0, fmt, ap_copy);
        va_end(ap_copy);
        if (len < 0) return;
        std::string msg(static_cast<std::size_t>(len) + 1, '\0');
        std::vsnprintf(msg.data(), msg.size(), fmt, ap);
        msg.resize(static_cast<std::size_t>(len));

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

    void init_default(const std::string &file_path, Level lv) {
        auto &logger = Logger::instance();
        logger.set_level(lv);
        logger.clear_sinks();
        logger.add_sink(make_stderr_sink(true));
        if (!file_path.empty()) {
            logger.add_sink(make_file_sink(file_path));
        }
    }

    std::shared_ptr<ISink> make_stderr_sink(bool color) {
        return std::make_shared<StderrSink>(color);
    }

    Level level_from_string(std::string_view s) noexcept {
        if (s == "trace") return Level::Trace;
        if (s == "debug") return Level::Debug;
        if (s == "info") return Level::Info;
        if (s == "warn" || s == "warning") return Level::Warn;
        if (s == "error") return Level::Error;
        if (s == "critical" || s == "crit") return Level::Critical;
        if (s == "off") return Level::Off;
        return Level::Info;
    }

    std::shared_ptr<ISink> make_file_sink(
        const std::string &path,
        std::size_t max_bytes,
        int max_files
    ) {
        return std::make_shared<FileSink>(path, max_bytes, max_files);
    }
}
