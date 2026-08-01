#include <QtTest>
#include "../src/views/log_model.h"

class TestLogModel : public QObject {
    Q_OBJECT

private slots:
    void init_test() {
        LogModel m;
        QCOMPARE(m.rowCount(), 0);
        QCOMPARE(m.total(), 0);
    }

    void append_and_count() {
        LogModel m;
        m.append("10:00:00", "INFO", "test message");
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.total(), 1);
    }

    void data_roles() {
        LogModel m;
        m.append("10:00:00", "WARN", "warning msg");
        QModelIndex idx = m.index(0);
        QCOMPARE(idx.data(LogModel::TimestampRole).toString(), "10:00:00");
        QCOMPARE(idx.data(LogModel::LevelRole).toString(), "WARN");
        QCOMPARE(idx.data(LogModel::MessageRole).toString(), "warning msg");
        QVERIFY(idx.data(LogModel::ColorRole).value<QColor>().isValid());
    }

    void color_by_level() {
        LogModel m;
        m.append("t", "CRIT", "x");
        QCOMPARE(m.index(0).data(LogModel::ColorRole).value<QColor>().name(), "#ff8a80");
        m.append("t", "ERROR", "x");
        QCOMPARE(m.index(1).data(LogModel::ColorRole).value<QColor>().name(), "#ffab91");
        m.append("t", "Info", "x");
        QCOMPARE(m.index(2).data(LogModel::ColorRole).value<QColor>().name(), "#f06292");
    }

    void explicit_color_override() {
        LogModel m;
        m.append("t", "INFO", "custom", QColor("#abcdef"));
        QCOMPARE(m.index(0).data(LogModel::ColorRole).value<QColor>().name(), "#abcdef");
    }

    void clear() {
        LogModel m;
        m.append("t", "INFO", "a");
        m.append("t", "INFO", "b");
        m.clear();
        QCOMPARE(m.rowCount(), 0);
        QCOMPARE(m.total(), 0);
    }

    void max_entries_prunes() {
        LogModel m;
        for (int i = 0; i < 6000; ++i)
            m.append("t", "INFO", QString::number(i));
        QVERIFY(m.rowCount() <= 5000);
    }

    void role_names() {
        LogModel m;
        auto names = m.roleNames();
        QVERIFY(names.contains(LogModel::TimestampRole));
        QVERIFY(names.contains(LogModel::LevelRole));
        QVERIFY(names.contains(LogModel::MessageRole));
        QVERIFY(names.contains(LogModel::ColorRole));
    }
};

QObject *make_test_log_model() { return new TestLogModel; }
#include "test_log_model.moc"
