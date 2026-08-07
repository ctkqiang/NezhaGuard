//
// Created by 钟智强 on 2026/8/7.
//

#include "tools_page.h"
#include "theme.h"

#include "../tools/tools.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtConcurrent>

ToolsPage::ToolsPage(bool dark, QWidget *parent) : QFrame(parent), dark_(dark) {
    build_ui();
}

void ToolsPage::set_dark(bool d) {
    dark_ = d;
    build_ui();
}

void ToolsPage::build_ui() {
    if (layout()) {
        QLayoutItem *child;
        while ((child = layout()->takeAt(0)) != nullptr)
            delete child->widget();
        delete layout();
    }

    auto Bg   = dark_ ? Theme::DkBg     : Theme::LtBg;
    auto Card = dark_ ? Theme::DkCard   : Theme::LtCard;
    auto Br   = dark_ ? Theme::DkBorder : Theme::LtBorder;
    auto Tx   = dark_ ? Theme::DkText   : Theme::LtText;
    auto Mu   = dark_ ? Theme::DkMuted  : Theme::LtMuted;
    auto Pk   = dark_ ? Theme::Strawberry : Theme::RosyDeep;

    auto *root = new QVBoxLayout(this);
    root->setSpacing(12);
    root->setContentsMargins(0, 0, 0, 0);

    // header + options
    auto *hdr = new QHBoxLayout();
    status_label_ = new QLabel("就绪");
    status_label_->setStyleSheet(
        QStringLiteral("font-size:11px; font-weight:600; color:%1; background:transparent;").arg(Pk));
    hdr->addWidget(status_label_);
    hdr->addStretch();

    auto *optLbl = new QLabel("行数上限:");
    optLbl->setStyleSheet(QStringLiteral("font-size:10px; color:%1; background:transparent;").arg(Mu));
    hdr->addWidget(optLbl);

    limit_spin_ = new QSpinBox();
    limit_spin_->setRange(10, 2000);
    limit_spin_->setValue(200);
    limit_spin_->setStyleSheet(QStringLiteral(
        "QSpinBox { background:%1; color:%2; border:1px solid %3; border-radius:8px; padding:4px 8px; font-size:10px; }"
    ).arg(Card, Tx, Br));
    hdr->addWidget(limit_spin_);

    pattern_edit_ = new QLineEdit();
    pattern_edit_->setPlaceholderText("搜索关键词…");
    pattern_edit_->setStyleSheet(QStringLiteral(
        "QLineEdit { background:%1; color:%2; border:1px solid %3; border-radius:8px; padding:4px 10px; font-size:10px; }"
    ).arg(Card, Tx, Br));
    pattern_edit_->setMaximumWidth(180);
    hdr->addWidget(pattern_edit_);

    refresh_btn_ = new QPushButton("执行");
    refresh_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:10px; padding:5px 16px; "
        "font-size:10px; font-weight:700; } QPushButton:hover { border-color:%2; }"
    ).arg(Card, Pk, Br));
    hdr->addWidget(refresh_btn_);
    root->addLayout(hdr);

    // splitter area
    auto *body = new QHBoxLayout();
    body->setSpacing(12);

    tool_list_ = new QListWidget();
    tool_list_->setMaximumWidth(160);
    tool_list_->setStyleSheet(QStringLiteral(
        "QListWidget { background:%1; color:%2; border:1px solid %3; border-radius:14px; "
        "font-size:11px; padding:4px; } QListWidget::item { padding:8px 14px; border-radius:8px; } "
        "QListWidget::item:selected { background:%4; color:%5; font-weight:700; } "
        "QListWidget::item:hover:!selected { color:%5; }"
    ).arg(Card, Tx, Br, dark_ ? Theme::DkSelected : Theme::LtSelected, Pk));

    for (auto id : Nezha::Tools::all_tool_ids())
        tool_list_->addItem(QString::fromUtf8(Nezha::Tools::tool_display_name(id)));
    tool_list_->setCurrentRow(4); // ps default
    body->addWidget(tool_list_);

    // right panel
    auto *right = new QVBoxLayout();
    right->setSpacing(8);

    result_table_ = new QTableWidget(0, 1);
    result_table_->setFrameShape(QFrame::NoFrame);
    result_table_->setAlternatingRowColors(true);
    result_table_->setShowGrid(false);
    result_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    result_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    result_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    result_table_->verticalHeader()->hide();
    result_table_->horizontalHeader()->setStretchLastSection(true);
    result_table_->setStyleSheet(QStringLiteral(
        "QTableWidget { background:%1; color:%2; border:1px solid %3; border-radius:14px; "
        "font-size:11px; font-family:\"Menlo\",monospace; } "
        "QTableWidget::item:selected { background:%4; } "
        "QHeaderView::section { background:%5; color:%6; border:none; border-bottom:1px solid %3; "
        "font-size:10px; font-weight:700; padding:5px 8px; }"
    ).arg(Card, Tx, Br, dark_ ? Theme::DkSelected : Theme::LtSelected, Card, Mu));
    right->addWidget(result_table_, 3);

    raw_view_ = new QTextEdit();
    raw_view_->setReadOnly(true);
    raw_view_->setMaximumHeight(160);
    raw_view_->setStyleSheet(QStringLiteral(
        "QTextEdit { background:%1; color:%6; border:1px solid %3; border-radius:14px; "
        "font-size:10px; font-family:\"Menlo\",monospace; padding:8px; }"
    ).arg(Bg, Card, Br, dark_ ? Theme::DkSelected : Theme::LtSelected, Card, Mu));
    raw_view_->setPlaceholderText("原始输出…");
    right->addWidget(raw_view_, 1);
    body->addLayout(right, 1);

    root->addLayout(body, 1);

    connect(tool_list_, &QListWidget::currentRowChanged, this, &ToolsPage::on_tool_selected);
    connect(refresh_btn_, &QPushButton::clicked, this, &ToolsPage::refresh);
    connect(pattern_edit_, &QLineEdit::returnPressed, this, &ToolsPage::refresh);

    watcher_ = new QFutureWatcher<Nezha::Tools::ToolResult>(this);
    connect(watcher_, &QFutureWatcher::finished, this, &ToolsPage::on_future_finished);

    refresh();
}

void ToolsPage::on_tool_selected(int row) {
    if (row < 0 || row >= static_cast<int>(Nezha::Tools::all_tool_ids().size())) return;
    current_tool_ = Nezha::Tools::all_tool_ids()[row];
    bool showPattern = (current_tool_ == Nezha::Tools::ToolId::Grep ||
                        current_tool_ == Nezha::Tools::ToolId::Journalctl ||
                        current_tool_ == Nezha::Tools::ToolId::Faillog);
    pattern_edit_->setVisible(showPattern);
    if (!showPattern) pattern_edit_->clear();
    refresh();
}

void ToolsPage::refresh() {
    if (running_) return;
    running_ = true;
    refresh_btn_->setEnabled(false);
    status_label_->setText("运行中…");

    Nezha::Tools::ToolOptions opts;
    opts.limit = limit_spin_->value();
    opts.pattern = pattern_edit_->text().toStdString();

    auto id = current_tool_;
    watcher_->setFuture(QtConcurrent::run([id, opts]() {
        auto tool = Nezha::Tools::make_tool(id);
        if (!tool) {
            Nezha::Tools::ToolResult err;
            err.id = id;
            err.ok = false;
            err.error = "工具未实现";
            return err;
        }
        return tool->run(opts);
    }));
}

void ToolsPage::on_future_finished() {
    running_ = false;
    refresh_btn_->setEnabled(true);

    auto result = watcher_->result();
    auto Pk = dark_ ? Theme::Strawberry : Theme::RosyDeep;
    auto Mu = dark_ ? Theme::DkMuted : Theme::LtMuted;

    if (!result.ok) {
        status_label_->setStyleSheet(
            QStringLiteral("font-size:11px; font-weight:600; color:%1; background:transparent;").arg(Theme::Coral));
        status_label_->setText(QString::fromStdString(result.error));
        result_table_->setRowCount(0);
        result_table_->setColumnCount(1);
        result_table_->setHorizontalHeaderLabels({"错误"});
        raw_view_->clear();
        return;
    }

    status_label_->setStyleSheet(
        QStringLiteral("font-size:11px; font-weight:600; color:%1; background:transparent;").arg(Pk));
    status_label_->setText(QString::fromStdString(result.summary));

    // fill table
    result_table_->setColumnCount(static_cast<int>(result.columns.size()));
    QStringList headers;
    for (const auto &col : result.columns)
        headers << QString::fromStdString(col);
    result_table_->setHorizontalHeaderLabels(headers);
    result_table_->setRowCount(static_cast<int>(std::min(result.rows.size(), size_t(500))));
    for (size_t r = 0; r < result.rows.size() && r < 500; ++r) {
        for (size_t c = 0; c < result.rows[r].size(); ++c) {
            auto *item = new QTableWidgetItem(QString::fromStdString(result.rows[r][c]));
            item->setForeground(QColor(c == 0 ? Pk : (dark_ ? Theme::DkText : Theme::LtText)));
            result_table_->setItem(static_cast<int>(r), static_cast<int>(c), item);
        }
    }
    result_table_->resizeColumnsToContents();

    // raw text
    raw_view_->setStyleSheet(QStringLiteral(
        "QTextEdit { background:%1; color:%2; border:1px solid %3; border-radius:14px; "
        "font-size:10px; font-family:\"Menlo\",monospace; padding:8px; }"
    ).arg(dark_ ? Theme::DkBg : Theme::LtBg,
          Mu,
          dark_ ? Theme::DkBorder : Theme::LtBorder));
    raw_view_->setText(QString::fromStdString(result.raw_text));
}