//
// Created by 钟智强 on 2026/7/30.
//

#include "capture.h"
#include <pcap.h>
#include <utility>

namespace Nezha::Core {
    std::vector<std::string> PacketCapture::list_devices() {
        std::vector<std::string> devs;
        pcap_if_t *alldevs = nullptr;
        char err[PCAP_ERRBUF_SIZE]{};
        if (pcap_findalldevs(&alldevs, err) == 0) {
            for (pcap_if_t *d = alldevs; d; d = d->next) {
                if (d->name) devs.emplace_back(d->name);
            }
            pcap_freealldevs(alldevs);
        }
        return devs;
    }

    PacketCapture::~PacketCapture() {
        if (handle_) pcap_close(static_cast<pcap_t *>(handle_));
    }

    PacketCapture::PacketCapture(PacketCapture &&o) noexcept
        : handle_(o.handle_), err_(std::move(o.err_)) {
        o.handle_ = nullptr;
    }

    PacketCapture &PacketCapture::operator=(PacketCapture &&o) noexcept {
        if (this != &o) {
            if (handle_) pcap_close(static_cast<pcap_t *>(handle_));
            handle_ = o.handle_;
            err_ = std::move(o.err_);
            o.handle_ = nullptr;
        }
        return *this;
    }

    bool PacketCapture::open(const std::string &iface, int snaplen, bool promisc, int timeout_ms) {
        if (handle_) {
            pcap_close(static_cast<pcap_t *>(handle_));
            handle_ = nullptr;
        }
        char errbuf[PCAP_ERRBUF_SIZE]{};
        handle_ = pcap_open_live(iface.c_str(), snaplen, promisc ? 1 : 0, timeout_ms, errbuf);
        if (!handle_) {
            err_ = errbuf;
            return false;
        }
        err_.clear();
        stop_ = false;
        return true;
    }

    bool PacketCapture::set_filter(const std::string &expr) {
        if (!handle_) return false;
        bpf_program prog{};

        if (pcap_compile(static_cast<pcap_t *>(handle_), &prog, expr.c_str(), 1, PCAP_NETMASK_UNKNOWN) != 0) {
            err_ = pcap_geterr(static_cast<pcap_t *>(handle_));
            return false;
        }

        if (pcap_setfilter(static_cast<pcap_t *>(handle_), &prog) != 0) {
            err_ = pcap_geterr(static_cast<pcap_t *>(handle_));
            pcap_freecode(&prog);
            return false;
        }

        pcap_freecode(&prog);
        return true;
    }

    namespace {
        struct LoopCtx {
            pcap_t *pcap;
            CaptureCallback *cb;
            volatile bool *stop;
        };
    }

    void PacketCapture::start(CaptureCallback cb) {
        if (!handle_ || !cb) return;
        stop_ = false;

        LoopCtx ctx{static_cast<pcap_t *>(handle_), &cb, &stop_};

        auto handler = [](u_char *user, const pcap_pkthdr *hdr, const u_char *bytes) {
            auto *c = reinterpret_cast<LoopCtx *>(user);

            if (*c->stop) {
                pcap_breakloop(c->pcap);
                return;
            }
            if (c->cb && *c->cb) {
                timeval tv{};
                tv.tv_sec = hdr->ts.tv_sec;
                tv.tv_usec = hdr->ts.tv_usec;
                (*c->cb)(bytes, hdr->caplen, tv);
            }
        };

        pcap_loop(static_cast<pcap_t *>(handle_), -1, handler, reinterpret_cast<u_char *>(&ctx));
    }

    void PacketCapture::stop() {
        stop_ = true;
        if (handle_) pcap_breakloop(static_cast<pcap_t *>(handle_));
    }
}
