#include <QtTest>
#include "../src/core/types.h"

using namespace Nezha;

class TestTypes : public QObject {
    Q_OBJECT

private slots:
    void severity_enum() {
        QCOMPARE(static_cast<int>(Severity::Trace), 0);
        QCOMPARE(static_cast<int>(Severity::Debug), 1);
        QCOMPARE(static_cast<int>(Severity::Info), 2);
        QCOMPARE(static_cast<int>(Severity::Warn), 3);
        QCOMPARE(static_cast<int>(Severity::Error), 4);
        QCOMPARE(static_cast<int>(Severity::Critical), 5);
    }

    void protocol_constants() {
        QCOMPARE(PROTO_ICMP, 1);
        QCOMPARE(PROTO_TCP, 6);
        QCOMPARE(PROTO_UDP, 17);
    }

    void event_source_enum() {
        QCOMPARE(static_cast<int>(EventSource::Packet), 0);
        QCOMPARE(static_cast<int>(EventSource::Log), 1);
        QCOMPARE(static_cast<int>(EventSource::Honeypot), 2);
    }

    void nanos_type() { QVERIFY(sizeof(Nanos) == 8); }
    void appid_type() { QVERIFY(sizeof(AppId) == 2); }
    void fieldid_type() { QVERIFY(sizeof(FieldId) == 4); }
};

QObject *make_test_types() { return new TestTypes; }
#include "test_types.moc"
