#ifndef NEZHAGUARD_MONITOR_H
#define NEZHAGUARD_MONITOR_H

#include <QMainWindow>
#include <QPointer>
#include <QFrame>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTime>
#include <QSet>
#include <QSystemTrayIcon>
#include <memory>
#include <string>
#include <unordered_map>

#include "../utilities/logger.h"

class LogModel;
class GuiSink;
class QSortFilterProxyModel;
class QLineEdit;
class QTableWidget;
class QTimer;
class QMenu;
class QAction;

QT_BEGIN_NAMESPACE
namespace Ui { class monitor; }
QT_END_NAMESPACE

class DetailPanel;
class RadarWidget;

class SparklineWidget : public QFrame {
    Q_OBJECT
public:
    using QFrame::QFrame;
    void set_data(const QList<int> &d);
    bool dark = true;
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QList<int> data_;
};

class LogDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    bool dark = true;
    void paint(QPainter *, const QStyleOptionViewItem &, const QModelIndex &) const override;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override;
};

class AlertDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    bool dark = true;
    void paint(QPainter *, const QStyleOptionViewItem &, const QModelIndex &) const override;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override;
};

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
    void record_attacker(const QString &ip, double score, const QString &type);

private slots:
    void apply_log_filter(int index);
    void filter_alert_severity(int index);
    void show_log_detail(const QModelIndex &idx);
    void show_alert_detail(const QModelIndex &idx);
    void show_honey_detail(const QModelIndex &idx);
    void refresh_network_info();
    void update_clock();
    void sync_theme();
    void load_settings_configs();
    void save_monitor_conf();
    void save_notifier_conf();
    void save_database_conf();
    void export_nzc();
    void import_nzc();
    void run_nmap_scan(const QString &ip);
    void tray_show();
    void tray_quit();
    void log_search_changed(const QString &text);
    void clear_logs();
    void update_sparkline();

private:
    void apply_theme(bool dark);
    void apply_stylesheet(bool dark);
    void setup_sidebar();
    void setup_tray();
    void setup_file_menu();
    void setup_log_table(QTableView *view);
    void setup_network_table(QTableWidget *table);
    void refresh_local_ips();
    void refresh_arp_table();
    void refresh_quarantine_list();
    void refresh_attackers();
    void refresh_quickstats();
    void start_animations();

    Ui::monitor *ui;
    LogModel *log_model_ = nullptr;
    LogModel *alert_model_ = nullptr;
    LogModel *honey_model_ = nullptr;
    std::shared_ptr<GuiSink> gui_sink_;
    QSortFilterProxyModel *log_proxy_ = nullptr;
    QSortFilterProxyModel *alert_proxy_ = nullptr;
    QSortFilterProxyModel *log_search_proxy_ = nullptr;
    QTimer *clock_timer_ = nullptr;
    QTimer *sparkline_timer_ = nullptr;
    LogDelegate *log_delegate_ = nullptr;
    AlertDelegate *alert_delegate_ = nullptr;
    AlertDelegate *recent_delegate_ = nullptr;
    LogDelegate *honey_delegate_ = nullptr;
    SparklineWidget *sparkline_widget_ = nullptr;
    RadarWidget *radar_widget_ = nullptr;
    DetailPanel *log_detail_panel_ = nullptr;
    DetailPanel *alert_detail_panel_ = nullptr;
    DetailPanel *honey_detail_panel_ = nullptr;
    struct Attacker { double score = 0; int count = 0; QString type; };
    QHash<QString, Attacker> attackers_;
    LogModel *attackers_model_ = nullptr;
    int sev_crit_ = 0, sev_error_ = 0, sev_warn_ = 0, sev_info_ = 0;
    QSet<QString> active_types_;
    int pkt_rate_ = 0;
    QList<int> sparkline_data_;
    QSystemTrayIcon *tray_ = nullptr;
    QMenu *tray_menu_ = nullptr;
    QMenu *file_menu_ = nullptr;
    QAction *export_act_ = nullptr;
    QAction *import_act_ = nullptr;
    bool dark_mode_ = true;
    QTime start_time_;
};

#endif //NEZHAGUARD_MONITOR_H
