#ifndef NEZHAGUARD_MONITOR_H
#define NEZHAGUARD_MONITOR_H

#include <QMainWindow>
#include <QPointer>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTime>
#include <memory>

#include "../utilities/logger.h"

class LogModel;
class GuiSink;
class QSortFilterProxyModel;
class QLineEdit;
class QListWidgetItem;
class QTableWidget;
class QTimer;

QT_BEGIN_NAMESPACE
namespace Ui { class monitor; }
QT_END_NAMESPACE

class monitor : public QMainWindow {
    Q_OBJECT

public:
    explicit monitor(QWidget *parent = nullptr);
    ~monitor() override;

    [[nodiscard]] LogModel *log_model() const noexcept { return log_model_; }
    [[nodiscard]] std::shared_ptr<GuiSink> gui_sink() const noexcept { return gui_sink_; }

    void init_models();

public slots:
    void update_stats(int log_count, int alert_count);
    void append_alert(const QString &time, const QString &type, const QString &ip,
                      int count, double score, const QString &severity);
    void append_honeypot(const QString &time, const QString &src_ip,
                         uint16_t sport, uint16_t dport, const QString &service);

private slots:
    void apply_log_filter(int index);
    void filter_alert_severity(int index);
    void show_log_detail(const QModelIndex &idx);
    void show_alert_detail(const QModelIndex &idx);
    void show_honey_detail(const QModelIndex &idx);
    void refresh_network_info();
    void update_clock();
    void sync_theme();
    void on_log_search_changed(const QString &text);
    void clear_logs();
    void update_sparkline();

private:
    void apply_theme(bool dark);
    void setup_sidebar();
    void setup_log_table(QTableView *view);
    void setup_network_table(QTableWidget *table);
    void refresh_local_ips();
    void refresh_arp_table();
    void refresh_quarantine_list();

    Ui::monitor *ui;
    LogModel *log_model_;
    LogModel *alert_model_;
    LogModel *honey_model_;
    std::shared_ptr<GuiSink> gui_sink_;
    QSortFilterProxyModel *log_proxy_;
    QSortFilterProxyModel *alert_proxy_;
    QTimer *clock_timer_;
    QStyledItemDelegate *log_delegate_;
    QStyledItemDelegate *alert_delegate_;
    QStyledItemDelegate *recent_delegate_;
    QStyledItemDelegate *honey_delegate_;
    QSortFilterProxyModel *log_search_proxy_ = nullptr;
    QTimer *sparkline_timer_ = nullptr;
    QFrame *sparkline_widget_ = nullptr;
    QList<int> sparkline_data_;
    int sparkline_pending_ = 0;
    bool dark_mode_ = true;
    QTime start_time_;
};

#endif //NEZHAGUARD_MONITOR_H
