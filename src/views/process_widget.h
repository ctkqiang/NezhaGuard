//
// Created by 钟智强 on 2026/8/8.
//

#ifndef NEZHAGUARD_PROCESS_WIDGET_H
#define NEZHAGUARD_PROCESS_WIDGET_H

#include <QFrame>
#include <QFutureWatcher>
#include <QTableWidget>
#include <QTimer>

#include "../tools/types.h"

class QLabel;

class ProcessWidget : public QFrame {
    Q_OBJECT
public:
    explicit ProcessWidget(bool dark, QWidget *parent = nullptr);
    void set_dark(bool d);

private slots:
    void refresh();
    void on_future_finished();

private:
    QTableWidget *table_ = nullptr;
    QLabel *status_label_ = nullptr;
    QFutureWatcher<Nezha::Tools::ToolResult> *watcher_ = nullptr;
    QTimer *timer_ = nullptr;
    bool dark_ = true;
    bool running_ = false;
};

#endif // NEZHAGUARD_PROCESS_WIDGET_H
