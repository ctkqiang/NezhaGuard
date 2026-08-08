//
// Created by 钟智强 on 2026/8/8.
//

#ifndef NEZHAGUARD_APP_SECURITY_PAGE_H
#define NEZHAGUARD_APP_SECURITY_PAGE_H

#include <QFrame>
#include <QFutureWatcher>
#include <QPointer>
#include <QString>
#include <vector>

class QListWidget;
class QPushButton;
class QTableWidget;
class QTextEdit;
class QLabel;
class QSpinBox;
class SpinnerWidget;

namespace Nezha::Core {
struct SbomFinding;
struct ShellcodeFinding;
struct ScanResult;
}

class AppSecurityPage : public QFrame {
    Q_OBJECT

public:
    explicit AppSecurityPage(bool dark, QWidget *parent = nullptr);
    ~AppSecurityPage() override;

    void set_dark(bool dark);

public slots:
    void refresh();

signals:
    void scan_complete(int sbom_count, int webshell_count, int shellcode_count, int total);

private:
    void build_ui();
    void on_scanner_selected(int row);
    void on_scan_finished();
    void render_results();

    bool dark_;
    QListWidget *scanner_list_ = nullptr;
    QTableWidget *result_table_ = nullptr;
    QTextEdit *raw_view_ = nullptr;
    QPushButton *scan_btn_ = nullptr;
    QLabel *summary_label_ = nullptr;
    QSpinBox *limit_spin_ = nullptr;
    SpinnerWidget *spinner_ = nullptr;
    QPointer<QFutureWatcher<void>> watcher_;

    // cached results from each scanner
    std::vector<Nezha::Core::SbomFinding> sbom_results_;
    std::vector<Nezha::Core::ShellcodeFinding> shellcode_results_;
    std::vector<Nezha::Core::ScanResult> webshell_results_;

    enum { TAB_SBOM = 0, TAB_WEBSHELL = 1, TAB_SHELLCODE = 2, TAB_ALL = 3 };
    int current_tab_ = TAB_ALL;
};

#endif // NEZHAGUARD_APP_SECURITY_PAGE_H