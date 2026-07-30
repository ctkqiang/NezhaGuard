//
// Created by 钟智强 on 2026/7/30.
//

#ifndef NEZHAGUARD_CAPTURE_H
#define NEZHAGUARD_CAPTURE_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <sys/time.h>

namespace Nezha::Core {
    using CaptureCallback = std::function<void(const std::uint8_t *raw, std::size_t len, const timeval &ts)>;

    // libpcap 采集器，RAII 管理句柄，不可拷贝，可移动
    class PacketCapture {
    public:
        static std::vector<std::string> list_devices();

        PacketCapture() = default;
        ~PacketCapture();

        PacketCapture(const PacketCapture &) = delete;
        PacketCapture &operator=(const PacketCapture &) = delete;
        PacketCapture(PacketCapture &&o) noexcept;
        PacketCapture &operator=(PacketCapture &&o) noexcept;

        bool open(const std::string &iface,
                  int snaplen = 65535,
                  bool promisc = true,
                  int timeout_ms = 1000);

        bool set_filter(const std::string &expr);

        // 阻塞循环，每收到一个包回调 cb，直到出错或 stop() 被调用
        void start(CaptureCallback cb);
        void stop();

        [[nodiscard]] bool is_open() const noexcept { return handle_ != nullptr; }
        [[nodiscard]] const std::string &error() const noexcept { return err_; }

    private:
        void *handle_ = nullptr; // pcap_t*
        std::string err_;
        volatile bool stop_ = false;
    };
}

#endif //NEZHAGUARD_CAPTURE_H
