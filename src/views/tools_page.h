//
// Created by 钟智强 on 2026/8/7.
//

#ifndef NEZHAGUARD_TOOLS_PAGE_H
#define NEZHAGUARD_TOOLS_PAGE_H

#include <QFrame>
#include <QFutureWatcher>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextEdit>

#include "../tools/types.h"

class QPushButton;
class QLabel;

class ToolsPage : public QFrame {
    Q_OBJECT
public:
    explicit ToolsPage(bool dark, QWidget *parent = nullptr);
    void set_dark(bool d);

private slots:
    void refresh();
    void on_tool_selected(int row);
    void on_future_finished();

private:
    void build_ui();

    QListWidget *tool_list_ = nullptr;
    QTableWidget *result_table_ = nullptr;
    QTextEdit *raw_view_ = nullptr;
    QPushButton *refresh_btn_ = nullptr;
    QLabel *status_label_ = nullptr;
    QSpinBox *limit_spin_ = nullptr;
    QLineEdit *pattern_edit_ = nullptr;

    QFutureWatcher<Nezha::Tools::ToolResult> *watcher_ = nullptr;
    Nezha::Tools::ToolId current_tool_ = Nezha::Tools::ToolId::Ps;
    bool dark_ = true;
    bool running_ = false;
};

#endif // NEZHAGUARD_TOOLS_PAGE_H