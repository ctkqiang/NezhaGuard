#include <QtTest>
#include "../src/core/arena.h"

using namespace Nezha::Core;

class TestArena : public QObject {
    Q_OBJECT

private slots:
    void alloc_basic() {
        Arena a(1024);
        void *p = a.allocate(64);
        QVERIFY(p != nullptr);
        QVERIFY(a.bytes_used() >= 64);
    }

    void alloc_aligned() {
        Arena a(1024);
        void *p = a.allocate(100, 16);
        QVERIFY(p != nullptr);
        QVERIFY((reinterpret_cast<uintptr_t>(p) % 16) == 0);
    }

    void alloc_triggers_new_block() {
        Arena a(128);
        QCOMPARE(a.block_count(), 1);
        a.allocate(200); // larger than default block
        QVERIFY(a.block_count() >= 2);
    }

    void intern_string() {
        Arena a(1024);
        auto sv = a.intern("hello arena");
        QCOMPARE(sv, "hello arena");
        QCOMPARE(sv.size(), 11);
    }

    void intern_empty() {
        Arena a(1024);
        auto sv = a.intern("");
        QVERIFY(sv.empty());
    }

    void intern_cstr() {
        Arena a(1024);
        const char *s = a.intern_cstr("cstring");
        QCOMPARE(std::string(s), "cstring");
        QCOMPARE(s[7], '\0');
    }

    void reset_reuses_memory() {
        Arena a(1024);
        void *p1 = a.allocate(64);
        a.reset();
        void *p2 = a.allocate(64);
        QCOMPARE(p1, p2); // same address after reset
    }

    void move_ctor() {
        Arena a(1024);
        void *p = a.allocate(32);
        std::size_t used = a.bytes_used();
        Arena b(std::move(a));
        QCOMPARE(b.bytes_used(), used);
        QVERIFY(a.bytes_used() == 0);
    }

    void move_assign() {
        Arena a(1024), b(256);
        a.allocate(64);
        std::size_t used = a.bytes_used();
        b = std::move(a);
        QCOMPARE(b.bytes_used(), used);
    }

    void create_trivial() {
        Arena a(1024);
        auto *n = a.create<int>(42);
        QVERIFY(n != nullptr);
        QCOMPARE(*n, 42);
    }
};

QObject *make_test_arena() { return new TestArena; }
#include "test_arena.moc"
