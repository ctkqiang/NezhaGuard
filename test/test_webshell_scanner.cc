#include <QtTest>
#include "../src/core/webshell_scanner.h"
#include "../src/core/arena.h"

#include <filesystem>
#include <fstream>

using namespace Nezha::Core;

class TestWebshellScanner : public QObject {
    Q_OBJECT

private:
    std::string tmpdir_;

    void write_file(const std::string &name, const std::string &content) {
        std::ofstream f(name);
        f << content;
    }

private slots:
    void initTestCase() {
        tmpdir_ = (std::filesystem::temp_directory_path() / "nezha_test_ws").string();
        std::filesystem::create_directories(tmpdir_);
    }

    void cleanupTestCase() {
        std::error_code ec;
        std::filesystem::remove_all(tmpdir_, ec);
    }

    void entropy_normal_file() {
        std::string normal = "<?php echo 'hello world'; ?>";
        double e = WebshellScanner::shannon_entropy(normal);
        QVERIFY(e < 4.5); // 正常代码熵值低
    }

    void entropy_obfuscated() {
        std::string obf = "<?php "
            "JHNiPQ0KW1UkZV0sSlRkKFtSZXNddFJpTiRzX0hFbkRFUi5bJGJdLlskZV0pKTsNCg=="
            "JGZrPUZ1bmN0aW9uIEVSUk9SfQ0KJGZrKCk7";
        double e = WebshellScanner::shannon_entropy(obf);
        QVERIFY(e > 4.0); // 混淆代码熵值高
    }

    void detect_eval_in_php() {
        std::string path = tmpdir_ + "/shell.php";
        write_file(path, "<?php eval(base64_decode($_POST['cmd'])); ?>");
        auto results = WebshellScanner::analyze_file(path, "<?php eval(base64_decode($_POST['cmd'])); ?>");
        QVERIFY(!results.empty());
        bool has_eval = false;
        for (const auto &r : results)
            if (r.matched_pattern == "eval(") { has_eval = true; break; }
        QVERIFY(has_eval);
    }

    void detect_system_call() {
        auto results = WebshellScanner::analyze_file("cmd.php", "<?php system('id'); ?>");
        bool has = false;
        for (const auto &r : results)
            if (r.matched_pattern == "system(") { has = true; break; }
        QVERIFY(has);
    }

    void detect_mini_shell() {
        // 一句话木马: 极小文件 + eval
        auto results = WebshellScanner::analyze_file("1.php", "<?php @eval($_POST['pwd']);?>");
        bool has_eval = false, has_mini = false;
        for (const auto &r : results) {
            if (r.matched_pattern == "eval(") has_eval = true;
            if (r.matched_pattern == "mini_shell") has_mini = true;
        }
        QVERIFY(has_eval);
        QVERIFY(has_mini);
    }

    void clean_file_no_threat() {
        auto results = WebshellScanner::analyze_file("clean.php",
            "<?php\n// Database config\n$host = 'localhost';\n$user = 'root';\n"
            "function get_users() { return []; }\n");
        QVERIFY(results.empty());
    }

    void is_webshell_ext_test() {
        QVERIFY(WebshellScanner::is_webshell_ext(".php"));
        QVERIFY(WebshellScanner::is_webshell_ext(".jsp"));
        QVERIFY(WebshellScanner::is_webshell_ext(".asp"));
        QVERIFY(!WebshellScanner::is_webshell_ext(".html"));
        QVERIFY(!WebshellScanner::is_webshell_ext(".jpg"));
    }

    void scan_empty_dir() {
        WebshellScanner ws;
        ws.add_target(tmpdir_, "test dir");
        // 清空目录后扫描
        for (const auto &e : std::filesystem::directory_iterator(tmpdir_))
            std::filesystem::remove_all(e.path());
        auto results = ws.scan_once();
        QCOMPARE(results.size(), 0);
    }

    void scan_with_shell_file() {
        std::string fpath = tmpdir_ + "/bad.php";
        write_file(fpath, "<?php eval($_GET['x']); system('id'); ?>");
        WebshellScanner ws;
        ws.add_target(fpath, "single file");  // 直接扫描文件而非目录
        auto results = ws.scan_once();
        QVERIFY(!results.empty());
    }
};

QObject *make_test_webshell_scanner() { return new TestWebshellScanner; }
#include "test_webshell_scanner.moc"
