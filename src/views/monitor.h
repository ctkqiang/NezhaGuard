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
class QListWidgetItem;
class QTableWidget;
class QTimer;
class QChart;

namespace QtCharts { class QChartView; class QLineSeries; class QDateTimeAxis; class QValueAxis; }

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
    void switch_page(int row);
    void apply_log_filter(int index);
    void filter_alert_severity(int index);
    void show_alert_detail(const QModelIndex &idx);
    void show_honey_detail(const QModelIndex &idx);
    void refresh_network_info();
    void update_clock();
    void sync_theme();

private:
    void apply_theme(bool dark);
    void setup_sidebar();
    void setup_log_table(QTableView *view);
    void setup_network_table(QTableWidget *table);
    void refresh_local_ips();
    void refresh_arp_table();
    void refresh_quarantine_list();
    void setup_chart();
    void update_chart(int alert_count);

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
    bool dark_mode_ = true;
    QTime start_time_;
    int chart_points_ = 0;
};

#endif //NEZHAGUARD_MONITOR_H
