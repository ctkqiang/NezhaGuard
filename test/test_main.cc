#include <QCoreApplication>
#include <QtTest>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    int status = 0;
    auto run = [&](QObject *t) { status |= QTest::qExec(t, argc, argv); };

    extern QObject *make_test_ipaddr();
    extern QObject *make_test_arena();
    extern QObject *make_test_types();
    extern QObject *make_test_log_model();
    extern QObject *make_test_theme();

    run(make_test_ipaddr());
    run(make_test_arena());
    run(make_test_types());
    run(make_test_log_model());
    run(make_test_theme());
    return status;
}