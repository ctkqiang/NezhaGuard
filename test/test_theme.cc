#include <QtTest>
#include "../src/views/theme.h"

class TestTheme : public QObject {
    Q_OBJECT

private slots:
    void colors_are_valid_hex() {
        auto valid = [](const char *s) {
            return s[0] == '#' && std::strlen(s) == 7;
        };
        QVERIFY(valid(Theme::Pink));
        QVERIFY(valid(Theme::PinkLight));
        QVERIFY(valid(Theme::PinkDeep));
        QVERIFY(valid(Theme::Rose));
        QVERIFY(valid(Theme::Red));
        QVERIFY(valid(Theme::Orange));
        QVERIFY(valid(Theme::Green));
        QVERIFY(valid(Theme::Purple));
        QVERIFY(valid(Theme::White));
        QVERIFY(valid(Theme::Grey));
    }

    void dark_colors_consistent() {
        QVERIFY(std::strlen(Theme::DkBg) == 7);
        QVERIFY(std::strlen(Theme::DkCard) == 7);
        QVERIFY(std::strlen(Theme::DkBorder) == 7);
        QVERIFY(std::strlen(Theme::DkText) == 7);
        QVERIFY(std::strlen(Theme::DkMuted) == 7);
    }

    void light_colors_consistent() {
        QVERIFY(std::strlen(Theme::LtBg) == 7);
        QVERIFY(std::strlen(Theme::LtCard) == 7);
        QVERIFY(std::strlen(Theme::LtBorder) == 7);
        QVERIFY(std::strlen(Theme::LtText) == 7);
        QVERIFY(std::strlen(Theme::LtMuted) == 7);
    }

    void pink_variants_different() {
        QVERIFY(std::strcmp(Theme::Pink, Theme::PinkDeep) != 0);
        QVERIFY(std::strcmp(Theme::Pink, Theme::PinkLight) != 0);
        QVERIFY(std::strcmp(Theme::PinkLight, Theme::PinkDeep) != 0);
    }
};

QObject *make_test_theme() { return new TestTheme; }
#include "test_theme.moc"
