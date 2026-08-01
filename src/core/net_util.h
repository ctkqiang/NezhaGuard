//
// Created by 钟智强 on 2026/7/31.
//

#ifndef NEZHAGUARD_NET_UTIL_H
#define NEZHAGUARD_NET_UTIL_H

namespace Nezha::Core {
    void dump_local_ips();
    void dump_arp_table();
    void dump_network_info();
    int arp_table_size();
}

#endif //NEZHAGUARD_NET_UTIL_H
