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
                            break; // 白色
                        case Level::Debug: color_code = "\033[36m";
                            break; // 青色
                        case Level::Info: color_code = "\033[32m";
                            break; // 绿色
                        case Level::Warn: color_code = "\033[33m";
                            break; // 黄色
                        case Level::Error: color_code = "\033[31m";
                            break; // 红色
                        case Level::Critical: color_code = "\033[41;37m";
                            break; // 红底白字
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
            FileSink(const std::string &path, std::size_t max_bytes, int max_files) : path_(path),
                max_bytes_(max_bytes), max_files_(max_files) {
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

    // 兼容旧式 printf 风格（内部转为字符串后转发）
    void Logger::vlogf(Level lv, const char *file, int line, const char *fmt, va_list ap) {
        if (!enabled(lv)) return;

        // 先用 vsnprintf 计算长度
        va_list ap_copy;
        va_copy(ap_copy, ap);
        int len = std::vsnprintf(nullptr, 0, fmt, ap_copy);
        va_end(ap_copy);

        if (len < 0) return; // 格式化失败

        std::string msg(static_cast<std::size_t>(len) + 1, '\0');
        std::vsnprintf(msg.data(), msg.size(), fmt, ap);
        msg.resize(static_cast<std::size_t>(len));

        // 组装完整日志行（类似 log() 的格式）
        std::string full = std::format(
            "[{}:{}] {}",
            file, line,
            msg
        );

        write_to_sinks(lv, full);
    }

    // ============================================================
    // 全局辅助函数实现（与 logger.h 声明完全匹配）
    // ============================================================
    void init_default(const std::string &file_path, Level lv) {
        auto &logger = Logger::instance();
        logger.set_level(lv);
        logger.clear_sinks();

        // 添加 stderr sink（彩色）
        logger.add_sink(make_stderr_sink(true));

        // 添加文件 sink
        if (!file_path.empty()) {
            logger.add_sink(make_file_sink(file_path));
        }
    }

    std::shared_ptr<ISink> make_stderr_sink(bool color) {
        return std::make_shared<StderrSink>(color);
    }

    std::shared_ptr<ISink> make_file_sink(
        const std::string &path,
        std::size_t max_bytes,
        int max_files
    ) {
        return std::make_shared<FileSink>(path, max_bytes, max_files);
    }
} // namespace Nezha::Log
