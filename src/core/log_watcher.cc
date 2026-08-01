//
// Created by 钟智强 on 2026/7/30.
//

#include "log_watcher.h"
#include "arena.h"
#include "ipaddr.h"
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>

namespace Nezha::Core {

    LogWatcher::~LogWatcher() { stop(); }

    void LogWatcher::add_source(const LogSource &src) { sources_.push_back(src); }

    void LogWatcher::start(Arena &arena, LogEventCallback cb) {
        if (running_) return;
        running_ = true;
        worker_ = std::thread([this, &arena, cb = std::move(cb)]() mutable {
            watch_loop(&arena, &cb);
        });
    }

    void LogWatcher::stop() {
        running_ = false;
        if (worker_.joinable()) worker_.join();
    }

    void LogWatcher::watch_loop(Arena *arena, LogEventCallback *cb) {
        struct FileState {
            std::string path;
            std::ifstream stream;
            std::streampos last_pos = 0;
        };
        std::vector<FileState> states;
        for (const auto &s: sources_) {
            FileState fs;
            fs.path = s.path;
            fs.stream.open(s.path);
            if (fs.stream.is_open()) {
                fs.stream.seekg(0, std::ios::end);
                fs.last_pos = fs.stream.tellg();
            }
            states.push_back(std::move(fs));
        }

        while (running_) {
            for (std::size_t i = 0; i < sources_.size() && running_; ++i) {
                auto &st = states[i];
                if (!st.stream.is_open()) {
                    st.stream.open(st.path);
                    if (st.stream.is_open()) {
                        st.stream.seekg(0, std::ios::end);
                        st.last_pos = st.stream.tellg();
                    }
                    continue;
                }
                st.stream.clear();
                st.stream.seekg(0, std::ios::end);
                auto cur_pos = st.stream.tellg();
                if (cur_pos < st.last_pos) {
                    // 日志轮转后文件截断，从头开始
                    st.stream.clear();
                    st.stream.seekg(0, std::ios::beg);
                    st.last_pos = 0;
                    cur_pos = st.stream.tellg();
                }
                if (cur_pos > st.last_pos) {
                    st.stream.seekg(st.last_pos);
                    std::string line;
                    while (std::getline(st.stream, line) && running_) {
                        if (line.empty()) continue;
                        event e{};
                        if (parse_line(line, *arena, sources_[i], e) && cb && *cb) {
                            (*cb)(e);
                        }
                    }
                    st.stream.clear();
                    st.last_pos = st.stream.tellg();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    bool LogWatcher::parse_line(std::string_view line, Arena &arena,
                                const LogSource &src, event &out) {
        out.source = src.source_type;
        out.app = src.app_id;
        out.ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

        if (parse_combined(line, arena, out)) return true;
        if (parse_syslog(line, arena, out)) return true;
        if (parse_auth(line, arena, out)) return true;
        if (parse_json(line, arena, out)) return true;

        out.msg = arena.intern(line);
        return true;
    }

    bool LogWatcher::parse_combined(std::string_view line, Arena &arena, event &out) {
        auto bracket = line.find("] \"");
        if (bracket == std::string_view::npos) return false;
        auto first_space = line.find(' ');
        if (first_space == std::string_view::npos) return false;

        std::string_view ip_str = line.substr(0, first_space);
        IPAddress::ipaddr ip;
        if (!IPAddress::ipaddr::parse(ip_str, ip)) return false;
        out.src = ip;
        out.proto = PROTO_TCP;

        auto ts_start = line.find('[');
        auto ts_end = line.find(']', ts_start);
        if (ts_start != std::string_view::npos && ts_end != std::string_view::npos) {
            std::string_view ts = line.substr(ts_start + 1, ts_end - ts_start - 1);
            if (ts.size() >= 20) {
                out.fields.put(2, FieldVal::str(arena.intern(ts)));
            }
        }

        auto req_start = line.find('"', ts_end);
        auto req_end = line.find('"', req_start + 1);
        if (req_start != std::string_view::npos && req_end != std::string_view::npos) {
            std::string_view req = line.substr(req_start + 1, req_end - req_start - 1);
            out.msg = arena.intern(req);

            auto sp1 = req.find(' ');
            if (sp1 != std::string_view::npos) {
                std::string_view method = req.substr(0, sp1);
                out.fields.put(0, FieldVal::str(arena.intern(method)));
                auto sp2 = req.find(' ', sp1 + 1);
                std::string_view path = (sp2 != std::string_view::npos)
                                        ? req.substr(sp1 + 1, sp2 - sp1 - 1)
                                        : req.substr(sp1 + 1);
                out.fields.put(3, FieldVal::str(arena.intern(path)));
            }
        }

        auto status_start = req_end + 2;
        if (status_start < line.size()) {
            auto status_end = line.find(' ', status_start);
            if (status_end != std::string_view::npos) {
                std::string_view status = line.substr(status_start, status_end - status_start);
                int sc = 0;
                for (char c : status) { if (c >= '0' && c <= '9') sc = sc * 10 + (c - '0'); else break; }
                out.fields.put(4, FieldVal::num(sc));
                if (!status.empty() && status[0] == '5') out.level = Severity::Warn;
                if (!status.empty() && status[0] == '4') out.level = Severity::Info;
            }

            auto ua_pos = line.rfind("\" \"");
            if (ua_pos != std::string_view::npos) {
                auto ua_end = line.rfind('"');
                if (ua_end > ua_pos + 3) {
                    std::string_view ua = line.substr(ua_pos + 3, ua_end - ua_pos - 3);
                    out.fields.put(5, FieldVal::str(arena.intern(ua)));
                }
            }
        }

        out.dport = 443;
        out.source = EventSource::Log;
        return true;
    }

    bool LogWatcher::parse_syslog(std::string_view line, Arena &arena, event &out) {
        if (line.size() < 15 || line[0] != '<') return false;
        auto end_pri = line.find('>');
        if (end_pri == std::string_view::npos || end_pri > 5) return false;

        auto after_pri = line.substr(end_pri + 1);

        int field = 0;
        std::size_t pos = 0;
        for (std::size_t i = 0; i < after_pri.size(); ++i) {
            if (after_pri[i] == ' ') {
                ++field;
                if (field == 3) {
                    auto msg_start = i + 1;
                    for (int j = 0; j < 2 && msg_start < after_pri.size(); ++msg_start) {
                        if (after_pri[msg_start - 1] == ' ') ++j;
                    }
                    if (msg_start < after_pri.size()) {
                        out.msg = arena.intern(after_pri.substr(msg_start));
                    }
                    break;
                }
            } else if (field == 2 && pos == 0) {
                pos = i;
            }
        }

        out.source = EventSource::Log;
        out.proto = PROTO_TCP;
        return true;
    }

    bool LogWatcher::parse_auth(std::string_view line, Arena &arena, event &out) {
        if (line.size() < 20) return false;
        if (line[3] != ' ' || line[6] != ' ') return false;

        auto from_pos = line.find("from ");
        if (from_pos != std::string_view::npos) {
            auto ip_start = from_pos + 5;
            auto ip_end = line.find(' ', ip_start);
            if (ip_end == std::string_view::npos) ip_end = line.size();
            std::string_view ip_str = line.substr(ip_start, ip_end - ip_start);
            IPAddress::ipaddr ip;
            if (IPAddress::ipaddr::parse(ip_str, ip)) {
                out.src = ip;
            }
        }

        if (line.find("Failed password") != std::string_view::npos) {
            out.level = Severity::Warn;
            out.fields.put(0, FieldVal::str(arena.intern("SSH_BRUTE")));
        } else if (line.find("Accepted password") != std::string_view::npos ||
                   line.find("Accepted publickey") != std::string_view::npos) {
            out.fields.put(0, FieldVal::str(arena.intern("SSH_LOGIN")));
        } else if (line.find("session opened") != std::string_view::npos) {
            out.fields.put(0, FieldVal::str(arena.intern("SESSION_OPEN")));
        }

        out.msg = arena.intern(line);
        out.source = EventSource::Log;
        out.proto = PROTO_TCP;
        out.dport = 22;
        return true;
    }

    bool LogWatcher::parse_json(std::string_view line, Arena &arena, event &out) {
        if (line.empty() || line[0] != '{') return false;

        auto extract_str = [&](const char *key, FieldId id) {
            std::string search = std::string("\"") + key + "\":\"";
            auto pos = line.find(search);
            if (pos == std::string_view::npos) {
                search = std::string("\"") + key + "\": \"";
                pos = line.find(search);
            }
            if (pos != std::string_view::npos) {
                auto val_start = pos + search.size();
                auto val_end = line.find('"', val_start);
                if (val_end != std::string_view::npos) {
                    out.fields.put(id, FieldVal::str(
                        arena.intern(line.substr(val_start, val_end - val_start))));
                }
            }
        };

        extract_str("ip", 6);
        extract_str("method", 0);
        extract_str("path", 3);
        extract_str("message", 7);

        const FieldVal *ip_val = out.fields.get(6);
        if (ip_val && ip_val->kind == FieldVal::Kind::Str) {
            IPAddress::ipaddr ip;
            if (IPAddress::ipaddr::parse(ip_val->s, ip)) out.src = ip;
        }

        out.msg = arena.intern(line);
        out.source = EventSource::Log;
        return true;
    }

}
