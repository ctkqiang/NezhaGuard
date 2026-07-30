#include <csignal>
#include <iostream>

#include "src/core/arena.h"
#include "src/core/capture.h"
#include "src/core/decoder.h"
#include "src/core/event.h"
#include "src/core/ipaddr.h"
#include "src/utilities/logger.h"

using namespace Nezha;

static Core::PacketCapture *g_cap = nullptr;

static void on_signal(int) {
    if (g_cap) g_cap->stop();
}

int main() {
    Log::init_default("logs/nezha.log", Log::Level::Info);

    auto devs = Core::PacketCapture::list_devices();
    if (devs.empty()) {
        NZ_ERROR("未找到网络接口");
        return 1;
    }

    NZ_INFO("可用网卡:");
    for (const auto &d: devs) NZ_INFO("  {}", d);

    Core::PacketCapture cap;
    g_cap = &cap;

    if (!cap.open(devs[0], 65535, true, 1000)) {
        NZ_ERROR("打开 {} 失败: {}", devs[0], cap.error());
        NZ_INFO("提示: 抓包需要 root 权限，请用 sudo 运行");
        return 1;
    }

    NZ_INFO("正在监听 {} …", devs[0]);

    if (!cap.set_filter("tcp or udp or icmp")) {
        NZ_WARN("BPF 过滤器设置失败: {}", cap.error());
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    Core::Arena arena(64 * 1024);
    std::uint64_t pkt_count = 0;

    cap.start([&](const std::uint8_t *raw, std::size_t len, const timeval &ts) {
        Core::event e{};
        if (!Core::ProtocolDecoder::decode(raw, len, ts, arena, e)) return;

        ++pkt_count;

        if (e.src == IPAddress::ipaddr{} && e.dst == IPAddress::ipaddr{}) return;

        bool pub = !e.src.is_private() || !e.dst.is_private();
        Log::Level lv = pub ? Log::Level::Info : Log::Level::Debug;

        if (Log::Logger::instance().enabled(lv)) {
            NZ_INFO("#{} {}", pkt_count, e.brief());
        }

        if (pkt_count % 10000 == 0) {
            arena.reset();
            NZ_DEBUG("arena 已重置 (pkt={})", pkt_count);
        }
    });

    NZ_INFO("已停止，共处理 {} 个包", pkt_count);
    Log::Logger::instance().flush();
    return 0;
}
