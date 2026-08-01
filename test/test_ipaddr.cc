#include <QtTest>
#include "../src/core/ipaddr.h"

using namespace Nezha::IPAddress;

class TestIpaddr : public QObject {
    Q_OBJECT

private slots:
    void parse_v4() {
        ipaddr a;
        QVERIFY(ipaddr::parse("192.168.1.1", a));
        QVERIFY(a.is_v4());
        QCOMPARE(a.to_string(), "192.168.1.1");
    }

    void parse_v6() {
        ipaddr a;
        QVERIFY(ipaddr::parse("::1", a));
        QVERIFY(!a.is_v4());
        QCOMPARE(a.to_string(), "::1");
    }

    void parse_v6_full() {
        ipaddr a;
        QVERIFY(ipaddr::parse("2001:db8::1", a));
        QCOMPARE(a.to_string(), "2001:db8::1");
    }

    void parse_empty_fails() {
        ipaddr a;
        QVERIFY(!ipaddr::parse("", a));
    }

    void parse_garbage_fails() {
        ipaddr a;
        QVERIFY(!ipaddr::parse("not_an_ip", a));
        QVERIFY(!ipaddr::parse("999.999.999.999", a));
    }

    void is_private_v4() {
        ipaddr a;
        QVERIFY(ipaddr::parse("10.0.0.1", a) && a.is_private());
        QVERIFY(ipaddr::parse("172.16.0.1", a) && a.is_private());
        QVERIFY(ipaddr::parse("192.168.1.1", a) && a.is_private());
        QVERIFY(ipaddr::parse("8.8.8.8", a) && !a.is_private());
    }

    void is_loopback() {
        ipaddr a;
        QVERIFY(ipaddr::parse("127.0.0.1", a) && a.is_loopback());
        QVERIFY(ipaddr::parse("127.99.88.77", a) && a.is_loopback());
        QVERIFY(ipaddr::parse("::1", a) && a.is_loopback());
        QVERIFY(ipaddr::parse("192.168.1.1", a) && !a.is_loopback());
    }

    void comparison() {
        ipaddr a, b;
        QVERIFY(ipaddr::parse("1.1.1.1", a));
        QVERIFY(ipaddr::parse("1.1.1.1", b));
        QCOMPARE(a, b);
        QVERIFY(ipaddr::parse("2.2.2.2", b));
        QVERIFY(a != b);
        QVERIFY(a < b);
    }

    void hash_consistent() {
        ipaddr a, b;
        ipaddr::parse("10.0.0.1", a);
        ipaddr::parse("10.0.0.1", b);
        ipaddr_hash h;
        QCOMPARE(h(a), h(b));
    }

    void from_v4_int() {
        auto a = ipaddr::from_v4(0xC0A80101); // 192.168.1.1
        QVERIFY(a.is_v4());
        QCOMPARE(a.to_string(), "192.168.1.1");
    }

    void bytes_roundtrip() {
        ipaddr a;
        ipaddr::parse("10.20.30.40", a);
        ipaddr b = ipaddr::from_bytes(a.bytes().data());
        QCOMPARE(a, b);
    }
};

QObject *make_test_ipaddr() { return new TestIpaddr; }
#include "test_ipaddr.moc"
