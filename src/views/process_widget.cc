//
// Created by 钟智强 on 2026/8/8.
//

#include "process_widget.h"
#include "theme.h"

#include "../tools/system_tool.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QVBoxLayout>
#include <QtConcurrent>

ProcessWidget::ProcessWidget(bool dark, QWidget *parent)
    : QFrame(parent), dark_(dark) {

    auto Bg   = dark_ ? Theme::DkBg     : Theme::LtBg;
    auto Card = dark_ ? Theme::DkCard   : Theme::LtCard;
    auto Br   = dark_ ? Theme::DkBorder : Theme::LtBorder;
    auto Tx   = dark_ ? Theme::DkText   : Theme::LtText;
    auto Mu   = dark_ ? Theme::DkMuted  : Theme::LtMuted;
    auto Pk   = dark_ ? Theme::Strawberry : Theme::RosyDeep;

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    auto *hdr = new QHBoxLayout();
    status_label_ = new QLabel("进程监控");
    status_label_->setStyleSheet(QStringLiteral(
        "font-size:12px; font-weight:700; color:%1; background:transparent;").arg(Pk));
    hdr->addWidget(status_label_);
    hdr->addStretch();
    root->addLayout(hdr);

    table_ = new QTableWidget(0, 4);
    table_->setFrameShape(QFrame::NoFrame);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->verticalHeader()->hide();
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setMaximumHeight(200);
    table_->setHorizontalHeaderLabels({"PID", "用户", "内存(MB)", "进程名"});
    table_->setStyleSheet(QStringLiteral(
        "QTableWidget { background:%1; color:%2; border:1px solid %3; border-radius:12px; "
        "font-size:10px; font-family:\"Menlo\",monospace; } "
        "QHeaderView::section { background:%4; color:%5; border:none; padding:3px 6px; font-size:9px; }"
    ).arg(Card, Tx, Br, Card, Mu));
    root->addWidget(table_);

    watcher_ = new QFutureWatcher<Nezha::Tools::ToolResult>(this);
    connect(watcher_, &QFutureWatcher<Nezha::Tools::ToolResult>::finished,
            this, &ProcessWidget::on_future_finished);

    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &ProcessWidget::refresh);
    timer_->start(5000); // refresh every 5s
    refresh();
}

void ProcessWidget::set_dark(bool d) {
    dark_ = d;
    // simplified, just update the existing widget
}

void ProcessWidget::refresh() {
    if (running_) return;
    running_ = true;
    watcher_->setFuture(QtConcurrent::run([]() {
        auto tool = Nezha::Tools::make_tool(Nezha::Tools::ToolId::Ps);
        if (!tool) {
            Nezha::Tools::ToolResult err;
            err.ok = false;
            return err;
        }
        Nezha::Tools::ToolOptions opts;
        opts.limit = 80;
        return tool->run(opts);
    }));
}

void ProcessWidget::on_future_finished() {
    running_ = false;
    auto result = watcher_->future().result();
    auto Pk = dark_ ? Theme::Strawberry : Theme::RosyDeep;
    auto Tx = dark_ ? Theme::DkText : Theme::LtText;
    auto Coral = Theme::Coral;

    // Skip header rows (first 2 cols: PID, PPID → we show PID, 用户, 内存, 进程名)
    // Tool columns: PID, PPID, 用户, 状态, 内存(MB), 命令
    table_->setRowCount(static_cast<int>(std::min(result.rows.size(), size_t(80))));
    for (size_t r = 0; r < result.rows.size() && r < 80; ++r) {
        const auto &row = result.rows[r];
        for (size_t c = 0; c < 4 && c < row.size(); ++c) {
            size_t srcCol = (c == 0) ? 0 : (c == 1) ? 2 : (c == 2) ? 4 : 5; // PID, 用户, 内存, 命令
            if (srcCol >= row.size()) continue;
            auto *item = new QTableWidgetItem(QString::fromStdString(row[srcCol]));
            // highlight suspicious: high memory or root
            bool suspicious = false;
            if (srcCol == 2 && row[srcCol] == "root") suspicious = true; // root user
            if (srcCol == 4) { // memory
                try { if (std::stod(row[srcCol]) > 500) suspicious = true; } catch (...) {}
            }
            item->setForeground(suspicious ? QColor(Coral) : QColor(Tx));
            table_->setItem(static_cast<int>(r), static_cast<int>(c), item);
        }
    }
    table_->resizeColumnsToContents();
    status_label_->setText(QString("进程监控 · %1 进程").arg(result.rows.size()));
}
