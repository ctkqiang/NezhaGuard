//
// Created by 钟智强 on 2026/8/8.
//

#include "app_security_page.h"
#include "theme.h"
#include "spinner_widget.h"
#include "../core/sbom_scanner.h"
#include "../core/shellcode_detector.h"
#include "../core/webshell_scanner.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QtConcurrent/QtConcurrent>

AppSecurityPage::AppSecurityPage(bool dark, QWidget *parent)
    : QFrame(parent), dark_(dark) {
    build_ui();
}

AppSecurityPage::~AppSecurityPage() {
    if (watcher_ && watcher_->isRunning()) {
        watcher_->cancel();
        watcher_->waitForFinished();
    }
}

void AppSecurityPage::set_dark(bool d) {
    dark_ = d;
    bool had_results = !sbom_results_.empty() || !shellcode_results_.empty() || !webshell_results_.empty();
    build_ui();
    if (had_results)
        render_results();
}

void AppSecurityPage::build_ui() {
    if (layout()) {
        QLayoutItem *child;
        while ((child = layout()->takeAt(0)) != nullptr)
            delete child->widget();
        delete layout();
    }

    auto Bg  = dark_ ? Theme::DkBg     : Theme::LtBg;
    auto Sh  = dark_ ? Theme::DkSheet  : Theme::LtSheet;
    auto Card = dark_ ? Theme::DkCard  : Theme::LtCard;
    auto Br  = dark_ ? Theme::DkBorder : Theme::LtBorder;
    auto Tx  = dark_ ? Theme::DkText   : Theme::LtText;
    auto Mu  = dark_ ? Theme::DkMuted  : Theme::LtMuted;
    auto Sel = dark_ ? Theme::DkSelected : Theme::LtSelected;
    auto Pk  = dark_ ? Theme::Strawberry : Theme::RosyDeep;

    // QFrame draws its bg from QPalette::Window — must set explicitly or it
    // ignores the parent QMainWindow stylesheet and shows system palette color.
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(Bg));
    setPalette(pal);
    setAutoFillBackground(true);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);

    // ── left sidebar ──
    auto *left_layout = new QVBoxLayout();
    left_layout->setSpacing(8);

    scanner_list_ = new QListWidget;
    scanner_list_->setMaximumWidth(180);
    scanner_list_->addItem(QStringLiteral("SBOM / CPE 扫描"));
    scanner_list_->addItem(QStringLiteral("Webshell 检测"));
    scanner_list_->addItem(QStringLiteral("Shellcode 检测"));
    scanner_list_->addItem(QStringLiteral("全部扫描"));
    scanner_list_->setCurrentRow(TAB_ALL);
    scanner_list_->setStyleSheet(QStringLiteral(
        "QListWidget { background:%1; color:%2; border:1px solid %3; border-radius:10px;"
        "font-size:12px; padding:4px; }"
        "QListWidget::item { padding:8px 12px; border-radius:8px; }"
        "QListWidget::item:selected { background:%4; color:%5; font-weight:700; }"
        "QListWidget::item:hover:!selected { color:%5; }"
    ).arg(Card, Tx, Br, Sel, Pk));
    left_layout->addWidget(scanner_list_);

    auto *limit_label = new QLabel(QStringLiteral("扫描上限"));
    limit_label->setStyleSheet(QStringLiteral("font-size:10px; color:%1; background:transparent;").arg(Mu));
    left_layout->addWidget(limit_label);

    limit_spin_ = new QSpinBox;
    limit_spin_->setRange(10, 5000);
    limit_spin_->setValue(500);
    limit_spin_->setStyleSheet(QStringLiteral(
        "QSpinBox { background:%1; color:%2; border:1px solid %3; border-radius:8px;"
        "padding:4px 8px; font-size:11px; }"
    ).arg(Card, Tx, Br));
    left_layout->addWidget(limit_spin_);

    scan_btn_ = new QPushButton(QStringLiteral("开始扫描"));
    scan_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:10px;"
        "padding:6px 16px; font-size:11px; font-weight:700; }"
        "QPushButton:hover { border-color:%2; }"
        "QPushButton:disabled { color:%4; }"
    ).arg(Card, Pk, Br, Mu));
    left_layout->addWidget(scan_btn_);

    left_layout->addStretch();

    summary_label_ = new QLabel;
    summary_label_->setWordWrap(true);
    summary_label_->setStyleSheet(QStringLiteral("font-size:11px; color:%1; background:transparent;").arg(Mu));
    left_layout->addWidget(summary_label_);

    // ── right content ──
    auto *right_layout = new QVBoxLayout();
    right_layout->setSpacing(8);

    // header
    result_table_ = new QTableWidget(0, 7);
    result_table_->setHorizontalHeaderLabels({
        QStringLiteral("类型"), QStringLiteral("严重级别"), QStringLiteral("分数"),
        QStringLiteral("文件 / 应用"), QStringLiteral("CVE / 模式"),
        QStringLiteral("版本"), QStringLiteral("威胁描述")
    });
    result_table_->setFrameShape(QFrame::NoFrame);
    result_table_->setAlternatingRowColors(true);
    result_table_->setShowGrid(false);
    result_table_->horizontalHeader()->setStretchLastSection(true);
    result_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    result_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    result_table_->verticalHeader()->hide();
    result_table_->setStyleSheet(QStringLiteral(
        "QTableWidget { background:%1; alternate-background-color:%2; color:%3;"
        "border:1px solid %4; border-radius:10px; gridline-color:%4;"
        "font-size:11px; font-family:\"Menlo\",monospace; }"
        "QTableWidget::item:selected { background:%5; }"
        "QHeaderView::section { background:%1; color:%6; border:none;"
        "border-bottom:1px solid %4; font-size:10px; font-weight:700; padding:5px 8px; }"
    ).arg(Card, dark_ ? Theme::DkBorder : Theme::LtBg, Tx, Br, Sel, Pk));
    right_layout->addWidget(result_table_, 3);

    raw_view_ = new QTextEdit;
    raw_view_->setReadOnly(true);
    raw_view_->setStyleSheet(QStringLiteral(
        "QTextEdit { background:%1; color:%2; border:1px solid %3; border-radius:10px;"
        "font-size:10px; font-family:\"Menlo\",monospace; padding:8px; }"
    ).arg(Sh, Mu, Br));
    raw_view_->setPlaceholderText(QStringLiteral("详细输出…"));
    right_layout->addWidget(raw_view_, 1);

    // ── spinner ──
    spinner_ = new SpinnerWidget(14, this);
    spinner_->dark = dark_;
    spinner_->setVisible(false);

    root->addLayout(left_layout);
    root->addLayout(right_layout, 1);

    // connections
    connect(scan_btn_, &QPushButton::clicked, this, &AppSecurityPage::refresh);
    connect(scanner_list_, &QListWidget::currentRowChanged, this, &AppSecurityPage::on_scanner_selected);
}

void AppSecurityPage::on_scanner_selected(int row) {
    current_tab_ = row;
    bool isSbom = (row == TAB_SBOM);
    limit_spin_->setVisible(!isSbom);

    if (row == TAB_WEBSHELL) {
        result_table_->setColumnCount(5);
        result_table_->setHorizontalHeaderLabels({
            QStringLiteral("文件路径"), QStringLiteral("匹配模式"),
            QStringLiteral("威胁描述"), QStringLiteral("分数"),
            QStringLiteral("严重级别")
        });
    } else if (row == TAB_SHELLCODE) {
        result_table_->setColumnCount(6);
        result_table_->setHorizontalHeaderLabels({
            QStringLiteral("文件路径"), QStringLiteral("偏移"),
            QStringLiteral("模式名称"), QStringLiteral("威胁描述"),
            QStringLiteral("分数"), QStringLiteral("Hex Dump")
        });
    } else {
        result_table_->setColumnCount(7);
        result_table_->setHorizontalHeaderLabels({
            QStringLiteral("类型"), QStringLiteral("严重级别"), QStringLiteral("分数"),
            QStringLiteral("文件 / 应用"), QStringLiteral("CVE / 模式"),
            QStringLiteral("版本"), QStringLiteral("威胁描述")
        });
    }
    result_table_->horizontalHeader()->setStretchLastSection(true);
}

void AppSecurityPage::refresh() {
    scan_btn_->setEnabled(false);
    scan_btn_->setText(QStringLiteral("扫描中…"));
    result_table_->setRowCount(0);
    raw_view_->clear();
    summary_label_->setText(QStringLiteral("正在扫描…"));
    spinner_->start();

    sbom_results_.clear();
    shellcode_results_.clear();
    webshell_results_.clear();

    int limit = limit_spin_->value();
    int tab = current_tab_;

    watcher_ = new QFutureWatcher<void>(this);
    connect(watcher_, &QFutureWatcher<void>::finished, this, &AppSecurityPage::on_scan_finished);

    QFuture<void> future = QtConcurrent::run([this, tab, limit] {
        if (tab == TAB_SBOM || tab == TAB_ALL) {
            try {
                Nezha::Core::SbomScanner sbom;
                sbom.add_target("/opt");
                sbom.add_target("/usr/local");
                sbom.add_target("/Applications");
                sbom_results_ = sbom.scan();
            } catch (...) {}
        }
        if (tab == TAB_WEBSHELL || tab == TAB_ALL) {
            try {
                Nezha::Core::WebshellScanner ws;
                ws.add_target("/var/www", "Web root");
                ws.add_target("/usr/share/nginx", "Nginx root");
                ws.add_target("/opt/homebrew/var/www", "Homebrew www");
                webshell_results_ = ws.scan_once();
            } catch (...) {}
        }
        if (tab == TAB_SHELLCODE || tab == TAB_ALL) {
            try {
                Nezha::Core::ShellcodeDetector sd;
                sd.add_target("/usr/local/bin");
                sd.add_target("/opt/homebrew/bin");
                sd.add_target("/tmp");
                shellcode_results_ = sd.scan();
                if (static_cast<int>(shellcode_results_.size()) > limit)
                    shellcode_results_.resize(limit);
            } catch (...) {}
        }
    });

    watcher_->setFuture(future);
}

void AppSecurityPage::on_scan_finished() {
    spinner_->stop();
    scan_btn_->setEnabled(true);
    scan_btn_->setText(QStringLiteral("开始扫描"));
    render_results();
}

void AppSecurityPage::render_results() {
    auto BrushCRIT = QColor(Theme::Strawberry);
    auto BrushERROR = QColor(Theme::Coral);
    auto BrushWARN  = QColor(Theme::SakuraLight);
    auto BrushINFO  = QColor(dark_ ? Theme::DkMuted : Theme::LtMuted);
    auto BrushText  = QColor(dark_ ? Theme::DkText : Theme::LtText);
    auto BrushVer   = QColor(dark_ ? Theme::Purple : Theme::RosyDeep);
    auto Pk = dark_ ? Theme::Strawberry : Theme::RosyDeep;

    auto color_for_score = [&](double s) -> QColor {
        if (s >= 85) return BrushCRIT;
        if (s >= 60) return BrushERROR;
        if (s >= 35) return BrushWARN;
        return BrushINFO;
    };

    auto sev_text = [](Nezha::Severity lvl) -> QString {
        switch (lvl) {
            case Nezha::Severity::Critical: return QStringLiteral("严重");
            case Nezha::Severity::Error:    return QStringLiteral("危险");
            case Nezha::Severity::Warn:     return QStringLiteral("警告");
            case Nezha::Severity::Info:     return QStringLiteral("信息");
            default:                        return QStringLiteral("未知");
        }
    };

    auto sev_color = [&](Nezha::Severity lvl) -> QColor {
        switch (lvl) {
            case Nezha::Severity::Critical: return BrushCRIT;
            case Nezha::Severity::Error:    return BrushERROR;
            case Nezha::Severity::Warn:     return BrushWARN;
            default:                        return BrushINFO;
        }
    };

    // restore header labels
    result_table_->setColumnCount(7);
    result_table_->setHorizontalHeaderLabels({
        QStringLiteral("类型"), QStringLiteral("严重级别"), QStringLiteral("分数"),
        QStringLiteral("文件 / 应用"), QStringLiteral("CVE / 模式"),
        QStringLiteral("版本"), QStringLiteral("威胁描述")
    });
    result_table_->horizontalHeader()->setStretchLastSection(true);

    std::vector<std::tuple<QString, QColor, double, QString, QString, QString, QString>> render_rows;

    for (const auto &f : sbom_results_) {
        render_rows.emplace_back(
            QStringLiteral("CVE"), sev_color(f.level), f.cvss_score * 10.0,
            QString::fromStdString(f.file_path),
            QString::fromStdString(f.cve_id),
            QString::fromStdString(f.component + " " + f.version),
            QString::fromStdString(f.description));
    }
    for (const auto &r : webshell_results_) {
        render_rows.emplace_back(
            QStringLiteral("Webshell"), sev_color(r.level), r.score,
            QString::fromStdString(r.file_path),
            QString::fromStdString(r.matched_pattern),
            QString(),
            QString::fromStdString(r.description));
    }
    for (const auto &f : shellcode_results_) {
        render_rows.emplace_back(
            QStringLiteral("Shellcode"), sev_color(f.level), f.score,
            QString::fromStdString(f.file_path),
            QString::fromStdString(f.pattern_name),
            QStringLiteral("0x%1").arg(f.offset, 8, 16, QLatin1Char('0')),
            QString::fromStdString(f.description));
    }

    std::ranges::sort(render_rows, [](const auto &a, const auto &b) {
        return std::get<2>(a) > std::get<2>(b);
    });

    int total = static_cast<int>(render_rows.size());
    result_table_->setRowCount(total);

    for (int i = 0; i < total; ++i) {
        const auto &[type, color, score, path, pattern, version, desc] = render_rows[i];

        auto *typeItem = new QTableWidgetItem(type);
        typeItem->setForeground(color);
        result_table_->setItem(i, 0, typeItem);

        auto *sevItem = new QTableWidgetItem(
            score >= 85 ? QStringLiteral("严重") :
            score >= 60 ? QStringLiteral("危险") :
            score >= 35 ? QStringLiteral("警告") : QStringLiteral("信息"));
        sevItem->setForeground(color_for_score(score));
        result_table_->setItem(i, 1, sevItem);

        auto *scoreItem = new QTableWidgetItem(QString::number(static_cast<int>(score)));
        scoreItem->setForeground(color_for_score(score));
        result_table_->setItem(i, 2, scoreItem);

        auto *pathItem = new QTableWidgetItem(path);
        pathItem->setForeground(BrushText);
        result_table_->setItem(i, 3, pathItem);

        auto *cveItem = new QTableWidgetItem(pattern);
        cveItem->setForeground(color_for_score(score));
        result_table_->setItem(i, 4, cveItem);

        auto *verItem = new QTableWidgetItem(version);
        verItem->setForeground(BrushVer);
        result_table_->setItem(i, 5, verItem);

        auto *descItem = new QTableWidgetItem(desc);
        descItem->setForeground(BrushText);
        result_table_->setItem(i, 6, descItem);
    }

    for (int c = 0; c < result_table_->columnCount(); ++c)
        result_table_->resizeColumnToContents(c);

    summary_label_->setStyleSheet(QStringLiteral(
        "font-size:11px; color:%1; background:transparent;").arg(Pk));
    summary_label_->setText(QStringLiteral(
        "SBOM/CVE: %1\nWebshell: %2\nShellcode: %3\n合计: %4")
        .arg(sbom_results_.size())
        .arg(webshell_results_.size())
        .arg(shellcode_results_.size())
        .arg(total));

    QString raw;
    for (const auto &[type, color, score, path, pattern, version, desc] : render_rows) {
        raw += QStringLiteral("[%1] %2 | %3 | %4 | %5\n  %6\n\n")
            .arg(type, QString::number(static_cast<int>(score)),
                 path, pattern, version, desc);
    }
    raw_view_->setPlainText(raw);

    emit scan_complete(
        static_cast<int>(sbom_results_.size()),
        static_cast<int>(webshell_results_.size()),
        static_cast<int>(shellcode_results_.size()), total);
}