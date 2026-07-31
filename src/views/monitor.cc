#include "monitor.h"
#include "ui_monitor.h"
#include "log_model.h"
#include "gui_sink.h"
#include "../service/database_helper.h"

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStyleHints>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

class LogDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    bool dark = true;

    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        QStyleOptionViewItem o = opt;
        initStyleOption(&o, idx);
        p->save();
        QColor bg = dark ? (idx.row() % 2 ? QColor("#141414") : QColor("#1a1a1a"))
                         : (idx.row() % 2 ? QColor("#fafafa") : QColor("#ffffff"));
        QColor fg = idx.data(LogModel::ColorRole).value<QColor>();
        if (!fg.isValid()) fg = dark ? QColor("#d9d9d9") : QColor("#434343");
        if (o.state & QStyle::State_Selected) { bg = dark ? QColor("#111d2c") : QColor("#e6f7ff"); }
        p->fillRect(o.rect, bg);
        QString tm = idx.data(LogModel::TimestampRole).toString();
        QString msg = idx.data(LogModel::MessageRole).toString();
        QString lv = idx.data(LogModel::LevelRole).toString();
        QFont mf(QStringLiteral("Menlo"), 10);
        mf.setStyleHint(QFont::Monospace);
        p->setFont(mf);
        QRect r = o.rect.adjusted(10, 2, -10, -2);
        int x = r.x();
        p->setPen(dark ? QColor("#595959") : QColor("#8c8c8c"));
        p->drawText(x, r.y(), r.width(), r.height(), Qt::AlignLeft | Qt::AlignVCenter, tm);
        x += p->fontMetrics().horizontalAdvance(tm) + 10;
        QRect bd(x, r.y() + 2, 56, r.height() - 4);
        p->setRenderHint(QPainter::Antialiasing);
        p->setBrush(fg.darker(dark ? 150 : 100));
        p->setPen(Qt::NoPen);
        p->drawRoundedRect(bd, 3, 3);
        p->setPen(dark ? fg.lighter(120) : QColor("#ffffff"));
        QFont bf = QApplication::font(); bf.setPointSize(8); bf.setBold(true);
        p->setFont(bf);
        p->drawText(bd, Qt::AlignCenter, lv);
        x += 66;
        p->setFont(mf);
        p->setPen(fg);
        p->drawText(QRect(x, r.y(), r.right() - x, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    p->fontMetrics().elidedText(msg, Qt::ElideRight, r.right() - x));
        p->restore();
    }
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return {200, 28}; }
};

class AlertDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    bool dark = true;

    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        QStyleOptionViewItem o = opt;
        initStyleOption(&o, idx);
        p->save();
        QColor bg = dark ? (idx.row() % 2 ? QColor("#141414") : QColor("#1a1a1a"))
                         : (idx.row() % 2 ? QColor("#fafafa") : QColor("#ffffff"));
        QColor fg = idx.data(LogModel::ColorRole).value<QColor>();
        if (!fg.isValid()) fg = dark ? QColor("#d9d9d9") : QColor("#434343");
        if (o.state & QStyle::State_Selected) { bg = dark ? QColor("#111d2c") : QColor("#e6f7ff"); }
        p->fillRect(o.rect, bg);
        QString tm = idx.data(LogModel::TimestampRole).toString();
        QString msg = idx.data(LogModel::MessageRole).toString();
        QString lv = idx.data(LogModel::LevelRole).toString();
        QFont mf(QStringLiteral("Menlo"), 10);
        mf.setStyleHint(QFont::Monospace);
        p->setFont(mf);
        QRect r = o.rect.adjusted(10, 2, -10, -2);
        int x = r.x();
        p->setPen(dark ? QColor("#595959") : QColor("#8c8c8c"));
        p->drawText(x, r.y(), r.width(), r.height(), Qt::AlignLeft | Qt::AlignVCenter, tm);
        x += p->fontMetrics().horizontalAdvance(tm) + 10;
        int lw = p->fontMetrics().horizontalAdvance(lv) + 14;
        QRect bd(x, r.y() + 2, lw, r.height() - 4);
        p->setRenderHint(QPainter::Antialiasing);
        p->setBrush(fg);
        p->setPen(Qt::NoPen);
        p->drawRoundedRect(bd, 3, 3);
        p->setPen(QColor("#ffffff"));
        QFont bf = QApplication::font(); bf.setPointSize(8); bf.setBold(true);
        p->setFont(bf);
        p->drawText(bd, Qt::AlignCenter, lv);
        x += lw + 10;
        p->setFont(mf);
        p->setPen(fg);
        p->drawText(QRect(x, r.y(), r.right() - x, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    p->fontMetrics().elidedText(msg, Qt::ElideRight, r.right() - x));
        p->restore();
    }
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return {200, 28}; }
};

} // namespace

monitor::monitor(QWidget *parent) : QMainWindow(parent), ui(new Ui::monitor) {
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, true);
    start_time_ = QTime::currentTime();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    dark_mode_ = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
#else
    dark_mode_ = (qApp->palette().color(QPalette::Window).lightness() < 128);
#endif
    apply_theme(dark_mode_);

    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, &monitor::sync_theme);

    clock_timer_ = new QTimer(this);
    connect(clock_timer_, &QTimer::timeout, this, &monitor::update_clock);
    clock_timer_->start(1000);
    update_clock();

    setup_sidebar();
}

monitor::~monitor() { delete ui; }

void monitor::sync_theme() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    bool dk = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
#else
    bool dk = (qApp->palette().color(QPalette::Window).lightness() < 128);
#endif
    if (dk != dark_mode_) {
        dark_mode_ = dk;
        apply_theme(dark_mode_);
    }
}

void monitor::apply_theme(bool dark) {
    if (auto *ld = dynamic_cast<LogDelegate *>(log_delegate_)) ld->dark = dark;
    if (auto *ad = dynamic_cast<AlertDelegate *>(alert_delegate_)) ad->dark = dark;
    if (auto *rd = dynamic_cast<AlertDelegate *>(recent_delegate_)) rd->dark = dark;
    if (auto *hd = dynamic_cast<LogDelegate *>(honey_delegate_)) hd->dark = dark;

    if (dark) {
        setStyleSheet(QStringLiteral(R"(
            QMainWindow { background:#000000; }
            #header { background:#0a0a0a; border-bottom:1px solid #1f1f1f; }
            QLabel { color:#d9d9d9; font-family:"PingFang SC","Microsoft YaHei",sans-serif; }
            #app_title { font-size:15px; font-weight:bold; color:#ffffff; }
            #clock_label { font-size:12px; color:#8c8c8c; margin-right:8px; }
            #status_dot { background:#49aa19; border-radius:4px; }
            #status_text { font-size:12px; color:#49aa19; }
            QStatusBar { background:#0a0a0a; border-top:1px solid #1f1f1f; }
            #sidebar { background:#0a0a0a; border-right:1px solid #1f1f1f; }
            #sidebar::item { color:#8c8c8c; padding:10px 20px; font-size:13px; border:none; margin:2px 8px; border-radius:6px; }
            #sidebar::item:selected { background:#111d2c; color:#177ddc; font-weight:bold; }
            #sidebar::item:hover:!selected { background:#141414; color:#d9d9d9; }
            #recent_alerts_label, #dash_title, #logs_title, #alerts_title, #honey_title, #network_title,
            #local_ip_label, #arp_label { font-size:14px; font-weight:bold; color:#ffffff; margin-bottom:4px; }
            QFrame#card_logs, QFrame#card_alerts, QFrame#card_threats, QFrame#card_uptime {
                background:#0a0a0a; border:1px solid #1f1f1f; border-radius:8px; padding:16px; }
            QFrame#card_logs:hover, QFrame#card_alerts:hover, QFrame#card_threats:hover, QFrame#card_uptime:hover {
                border-color:#177ddc; }
            #card_logs_value, #card_alerts_value, #card_threats_value, #card_uptime_value {
                font-size:28px; font-weight:bold; color:#ffffff; }
            #card_logs_icon, #card_alerts_icon, #card_threats_icon, #card_uptime_icon { font-size:20px; }
            #card_logs_label, #card_alerts_label, #card_threats_label, #card_uptime_label {
                font-size:12px; color:#8c8c8c; }
            QTableView { background:#000000; alternate-background-color:#0a0a0a; gridline-color:#1f1f1f;
                         color:#d9d9d9; border:1px solid #1f1f1f; border-radius:6px; font-size:11px; }
            QTableView::item:selected { background:#111d2c; }
            QHeaderView::section { background:#0a0a0a; color:#8c8c8c; padding:6px 12px;
                                   border:none; border-bottom:1px solid #1f1f1f; font-size:11px; font-weight:bold; }
            QComboBox { background:#0a0a0a; color:#d9d9d9; border:1px solid #434343; border-radius:6px;
                        padding:6px 12px; font-size:12px; min-width:90px; }
            QComboBox:hover { border-color:#177ddc; }
            QComboBox::drop-down { border:none; width:20px; }
            QComboBox QAbstractItemView { background:#141414; color:#d9d9d9;
                                          selection-background-color:#111d2c; border:1px solid #434343; }
            QPushButton { background:#111d2c; color:#177ddc; border:1px solid #15325b; border-radius:6px;
                          padding:6px 16px; font-size:12px; }
            QPushButton:hover { background:#15325b; border-color:#177ddc; color:#3c9ae8; }
            QTableWidget { background:#000000; gridline-color:#1f1f1f; color:#d9d9d9;
                           border:1px solid #1f1f1f; border-radius:6px; font-size:12px; }
            QTableWidget::item:selected { background:#111d2c; }
            QScrollBar:vertical { background:transparent; width:5px; margin:0; }
            QScrollBar::handle:vertical { background:#434343; border-radius:2px; min-height:20px; }
            QScrollBar::handle:vertical:hover { background:#595959; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        )"));
    } else {
        setStyleSheet(QStringLiteral(R"(
            QMainWindow { background:#f5f5f5; }
            #header { background:#ffffff; border-bottom:1px solid #e8e8e8; }
            QLabel { color:#434343; font-family:"PingFang SC","Microsoft YaHei",sans-serif; }
            #app_title { font-size:15px; font-weight:bold; color:#000000; }
            #clock_label { font-size:12px; color:#8c8c8c; margin-right:8px; }
            #status_dot { background:#52c41a; border-radius:4px; }
            #status_text { font-size:12px; color:#52c41a; }
            QStatusBar { background:#ffffff; border-top:1px solid #e8e8e8; }
            #sidebar { background:#ffffff; border-right:1px solid #e8e8e8; }
            #sidebar::item { color:#595959; padding:10px 20px; font-size:13px; border:none; margin:2px 8px; border-radius:6px; }
            #sidebar::item:selected { background:#e6f7ff; color:#1677ff; font-weight:bold; }
            #sidebar::item:hover:!selected { background:#fafafa; color:#434343; }
            #recent_alerts_label, #dash_title, #logs_title, #alerts_title, #honey_title, #network_title,
            #local_ip_label, #arp_label { font-size:14px; font-weight:bold; color:#000000; margin-bottom:4px; }
            QFrame#card_logs, QFrame#card_alerts, QFrame#card_threats, QFrame#card_uptime {
                background:#ffffff; border:1px solid #e8e8e8; border-radius:8px; padding:16px; }
            QFrame#card_logs:hover, QFrame#card_alerts:hover, QFrame#card_threats:hover, QFrame#card_uptime:hover {
                border-color:#1677ff; }
            #card_logs_value, #card_alerts_value, #card_threats_value, #card_uptime_value {
                font-size:28px; font-weight:bold; color:#000000; }
            #card_logs_icon, #card_alerts_icon, #card_threats_icon, #card_uptime_icon { font-size:20px; }
            #card_logs_label, #card_alerts_label, #card_threats_label, #card_uptime_label {
                font-size:12px; color:#8c8c8c; }
            QTableView { background:#ffffff; alternate-background-color:#fafafa; gridline-color:#f0f0f0;
                         color:#434343; border:1px solid #e8e8e8; border-radius:6px; font-size:11px; }
            QTableView::item:selected { background:#e6f7ff; color:#000000; }
            QHeaderView::section { background:#fafafa; color:#595959; padding:6px 12px;
                                   border:none; border-bottom:1px solid #e8e8e8; font-size:11px; font-weight:bold; }
            QComboBox { background:#ffffff; color:#434343; border:1px solid #d9d9d9; border-radius:6px;
                        padding:6px 12px; font-size:12px; min-width:90px; }
            QComboBox:hover { border-color:#1677ff; }
            QComboBox::drop-down { border:none; width:20px; }
            QComboBox QAbstractItemView { background:#ffffff; color:#434343;
                                          selection-background-color:#e6f7ff; border:1px solid #d9d9d9; }
            QPushButton { background:#ffffff; color:#1677ff; border:1px solid #1677ff; border-radius:6px;
                          padding:6px 16px; font-size:12px; }
            QPushButton:hover { background:#e6f7ff; color:#0958d9; border-color:#0958d9; }
            QTableWidget { background:#ffffff; gridline-color:#f0f0f0; color:#434343;
                           border:1px solid #e8e8e8; border-radius:6px; font-size:12px; }
            QTableWidget::item:selected { background:#e6f7ff; }
            QScrollBar:vertical { background:transparent; width:5px; margin:0; }
            QScrollBar::handle:vertical { background:#d9d9d9; border-radius:2px; min-height:20px; }
            QScrollBar::handle:vertical:hover { background:#bfbfbf; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        )"));
    }
}

void monitor::setup_sidebar() {
    ui->sidebar->setCurrentRow(0);
}

void monitor::update_clock() {
    ui->clock_label->setText(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd  hh:mm:ss")));
}

void monitor::setup_log_table(QTableView *view) {
    view->setShowGrid(false);
    view->setAlternatingRowColors(false);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->verticalHeader()->setDefaultSectionSize(28);
    view->verticalHeader()->hide();
    view->setMouseTracking(true);
}

void monitor::setup_network_table(QTableWidget *table) {
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
}

void monitor::init_models() {
    log_model_ = new LogModel(this);
    gui_sink_ = std::make_shared<GuiSink>(log_model_);

    log_proxy_ = new QSortFilterProxyModel(this);
    log_proxy_->setSourceModel(log_model_);
    log_proxy_->setFilterRole(LogModel::LevelRole);
    log_proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);

    setup_log_table(ui->log_view);
    ui->log_view->setModel(log_proxy_);
    ui->log_view->setColumnHidden(1, true);
    log_delegate_ = new LogDelegate(this);
    static_cast<LogDelegate *>(log_delegate_)->dark = dark_mode_;
    ui->log_view->setItemDelegate(log_delegate_);
    ui->log_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->log_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    connect(ui->level_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &monitor::apply_log_filter);

    alert_model_ = new LogModel(this);
    alert_proxy_ = new QSortFilterProxyModel(this);
    alert_proxy_->setSourceModel(alert_model_);
    alert_proxy_->setFilterRole(LogModel::LevelRole);
    alert_proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);

    setup_log_table(ui->alert_view);
    ui->alert_view->setModel(alert_proxy_);
    alert_delegate_ = new AlertDelegate(this);
    static_cast<AlertDelegate *>(alert_delegate_)->dark = dark_mode_;
    ui->alert_view->setItemDelegate(alert_delegate_);
    ui->alert_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->alert_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(ui->alert_view, &QTableView::clicked, this, &monitor::show_alert_detail);
    connect(ui->alert_severity_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &monitor::filter_alert_severity);

    setup_log_table(ui->recent_alerts_view);
    ui->recent_alerts_view->setModel(alert_model_);
    recent_delegate_ = new AlertDelegate(this);
    static_cast<AlertDelegate *>(recent_delegate_)->dark = dark_mode_;
    ui->recent_alerts_view->setItemDelegate(recent_delegate_);
    ui->recent_alerts_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->recent_alerts_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(ui->recent_alerts_view, &QTableView::clicked, this, &monitor::show_alert_detail);

    honey_model_ = new LogModel(this);
    setup_log_table(ui->honey_view);
    ui->honey_view->setModel(honey_model_);
    honey_delegate_ = new LogDelegate(this);
    static_cast<LogDelegate *>(honey_delegate_)->dark = dark_mode_;
    ui->honey_view->setItemDelegate(honey_delegate_);
    ui->honey_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->honey_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(ui->honey_view, &QTableView::clicked, this, &monitor::show_honey_detail);

    setup_network_table(ui->local_ip_table);
    setup_network_table(ui->arp_table);
    setup_network_table(ui->quarantine_table);
    connect(ui->refresh_network, &QPushButton::clicked, this, &monitor::refresh_network_info);
    refresh_network_info();
}

void monitor::update_stats(int log_count, int alert_count) {
    ui->app_title->setText(QStringLiteral("哪吒网络安全 SIEM"));
    int qcount = static_cast<int>(Nezha::Database::DatabaseHelper::GetQuarantineList().size());
    ui->status_label->setText(
        QStringLiteral("运行中  |  日志 %1  |  告警 %2  |  已隔离 %3")
            .arg(log_count).arg(alert_count).arg(qcount));
    ui->card_logs_value->setText(QString::number(log_count));
    ui->card_alerts_value->setText(QString::number(alert_count));
    ui->card_threats_value->setText(QString::number(qcount));

    int secs = start_time_.secsTo(QTime::currentTime());
    int h = secs / 3600, m = (secs % 3600) / 60, s = secs % 60;
    ui->card_uptime_value->setText(QStringLiteral("%1h %2m %3s").arg(h).arg(m).arg(s));
}

void monitor::append_alert(const QString &time, const QString &type, const QString &ip,
                           int count, double score, const QString &severity) {
    if (!alert_model_) return;
    // 丰富的中文告警信息
    QString sc = severity.startsWith(QStringLiteral("CRIT")) ? QStringLiteral("严重")
               : severity.startsWith(QStringLiteral("ERR"))  ? QStringLiteral("错误")
               : severity.startsWith(QStringLiteral("WARN")) ? QStringLiteral("警告")
               :                                               QStringLiteral("信息");
    QString msg = QStringLiteral("%1  |  %2  |  x%3  |  %4分")
                      .arg(type, ip).arg(count).arg(static_cast<int>(score));
    alert_model_->append(time, severity, msg);
    int a = alert_model_->total();
    ui->card_alerts_value->setText(QString::number(a));
    ui->card_threats_value->setText(QString::number(a));
    ui->alert_stats_label->setText(
        QStringLiteral("共 %1 条告警").arg(a));
}

void monitor::append_honeypot(const QString &time, const QString &src_ip,
                              uint16_t sport, uint16_t dport, const QString &service) {
    if (!honey_model_) return;
    QString msg = QStringLiteral("来源 %1:%2  →  端口 :%3  [%4]")
                      .arg(src_ip).arg(sport).arg(dport).arg(service);
    honey_model_->append(time, QStringLiteral("INFO"), msg);
    ui->honey_stats_label->setText(
        QStringLiteral("共 %1 次连接").arg(honey_model_->total()));
}

void monitor::apply_log_filter(int index) {
    if (!log_proxy_) return;
    log_proxy_->setFilterFixedString(index == 0 ? QString() : ui->level_filter->currentText());
}

void monitor::filter_alert_severity(int index) {
    if (!alert_proxy_) return;
    alert_proxy_->setFilterFixedString(index == 0 ? QString() : ui->alert_severity_filter->currentText());
}

void monitor::show_alert_detail(const QModelIndex &idx) {
    if (!idx.isValid()) return;
    QString tm = idx.data(LogModel::TimestampRole).toString();
    QString lv = idx.data(LogModel::LevelRole).toString();
    QString msg = idx.data(LogModel::MessageRole).toString();
    ui->alert_detail->setPlainText(
        QStringLiteral("时间: %1  |  级别: %2\n%3").arg(tm, lv, msg));
}

void monitor::show_honey_detail(const QModelIndex &idx) {
    if (!idx.isValid()) return;
    QString tm = idx.data(LogModel::TimestampRole).toString();
    QString msg = idx.data(LogModel::MessageRole).toString();
    ui->honey_detail->setPlainText(
        QStringLiteral("时间: %1\n%2").arg(tm, msg));
}

void monitor::refresh_local_ips() {
    auto *t = ui->local_ip_table;
    t->setRowCount(0);
    t->setHorizontalHeaderLabels({QStringLiteral("接口名称"), QStringLiteral("IP 地址")});
    ifaddrs *ifap = nullptr;
    if (getifaddrs(&ifap) != 0) return;
    QFont mf(QStringLiteral("Menlo"), 10);
    mf.setStyleHint(QFont::Monospace);
    for (ifaddrs *ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || (ifa->ifa_flags & IFF_LOOPBACK)) continue;
        int f = ifa->ifa_addr->sa_family;
        char b[INET6_ADDRSTRLEN] = {0};
        const char *ip = nullptr;
        if (f == AF_INET) {
            auto *s = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
            ip = inet_ntop(AF_INET, &s->sin_addr, b, sizeof(b));
        }
        if (!ip) continue;
        int r = t->rowCount(); t->insertRow(r);
        auto *n = new QTableWidgetItem(ifa->ifa_name); n->setForeground(QColor("#1677ff"));
        auto *i = new QTableWidgetItem(ip); i->setFont(mf); i->setForeground(QColor("#52c41a"));
        t->setItem(r, 0, n); t->setItem(r, 1, i);
    }
    freeifaddrs(ifap);
    t->resizeColumnToContents(0);
    t->horizontalHeader()->setStretchLastSection(true);
}

void monitor::refresh_arp_table() {
    auto *t = ui->arp_table;
    t->setRowCount(0);
    t->setHorizontalHeaderLabels({QStringLiteral("IP 地址"), QStringLiteral("MAC 地址")});
    std::set<std::string> local;
    ifaddrs *ifap = nullptr;
    if (getifaddrs(&ifap) == 0) {
        for (ifaddrs *ifa = ifap; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            char b[INET_ADDRSTRLEN] = {0};
            auto *s = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
            inet_ntop(AF_INET, &s->sin_addr, b, sizeof(b));
            local.insert(b);
        }
        freeifaddrs(ifap);
    }
    int mib[6] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO};
    std::size_t n = 0;
    if (sysctl(mib, 6, nullptr, &n, nullptr, 0) != 0 || n == 0) return;
    std::vector<char> buf(n * 2);
    n = buf.size();
    if (sysctl(mib, 6, buf.data(), &n, nullptr, 0) != 0) return;
    std::set<std::string> seen;
    QFont mf(QStringLiteral("Menlo"), 10);
    mf.setStyleHint(QFont::Monospace);
    for (char *p = buf.data(); p < buf.data() + n;) {
        auto *rtm = reinterpret_cast<rt_msghdr *>(p);
        if (rtm->rtm_version != RTM_VERSION) break;
        if (!(rtm->rtm_flags & RTF_LLINFO) || (rtm->rtm_flags & (RTF_LOCAL | RTF_BROADCAST | RTF_MULTICAST))) {
            p += rtm->rtm_msglen; continue;
        }
        auto *sa = reinterpret_cast<sockaddr *>(rtm + 1);
        int addrs = rtm->rtm_addrs;
        char ipb[INET_ADDRSTRLEN] = {0}, mb[18] = {0};
        const char *ips = nullptr;
        bool hm = false;
        for (int i = 0; i < RTAX_MAX; ++i) {
            if (!(addrs & (1 << i))) continue;
            int sl = sa->sa_len > 0 ? sa->sa_len : static_cast<uint8_t>(sizeof(sockaddr));
            if (i == RTAX_DST && sa->sa_family == AF_INET) {
                auto *sin = reinterpret_cast<sockaddr_in *>(sa);
                ips = inet_ntop(AF_INET, &sin->sin_addr, ipb, sizeof(ipb));
            }
            if (i == RTAX_GATEWAY && sa->sa_family == AF_LINK) {
                auto *sdl = reinterpret_cast<sockaddr_dl *>(sa);
                if (sdl->sdl_alen == 6) {
                    const auto *m = reinterpret_cast<const uint8_t *>(LLADDR(sdl));
                    snprintf(mb, sizeof(mb), "%02x:%02x:%02x:%02x:%02x:%02x",
                             m[0], m[1], m[2], m[3], m[4], m[5]);
                    hm = true;
                }
            }
            sa = reinterpret_cast<sockaddr *>(reinterpret_cast<char *>(sa) + sl);
        }
        if (!ips || !hm) { p += rtm->rtm_msglen; continue; }
        if (mb[0] == 'f' && mb[1] == 'f') { p += rtm->rtm_msglen; continue; }
        if ((mb[0] == '0' && mb[1] == '0') || (mb[1] & 1)) { p += rtm->rtm_msglen; continue; }
        if (local.count(ips)) { p += rtm->rtm_msglen; continue; }
        std::string key = std::string(ips) + "@" + mb;
        if (!seen.insert(key).second) { p += rtm->rtm_msglen; continue; }
        int row = t->rowCount(); t->insertRow(row);
        auto *ipx = new QTableWidgetItem(ips); ipx->setFont(mf); ipx->setForeground(QColor("#52c41a"));
        auto *mx = new QTableWidgetItem(mb); mx->setFont(mf); mx->setForeground(QColor("#8c8c8c"));
        t->setItem(row, 0, ipx); t->setItem(row, 1, mx);
        p += rtm->rtm_msglen;
    }
    t->resizeColumnToContents(0);
    t->horizontalHeader()->setStretchLastSection(true);
}

void monitor::refresh_network_info() {
    refresh_local_ips();
    refresh_arp_table();
    refresh_quarantine_list();
}

void monitor::refresh_quarantine_list() {
    auto list = Nezha::Database::DatabaseHelper::GetQuarantineList();
    ui->card_threats_value->setText(QString::number(static_cast<int>(list.size())));

    auto *t = ui->quarantine_table;
    t->setRowCount(0);
    t->setHorizontalHeaderLabels({QStringLiteral("IP 地址"), QStringLiteral("隔离原因"), QStringLiteral("威胁评分")});
    QFont mf(QStringLiteral("Menlo"), 10);
    mf.setStyleHint(QFont::Monospace);
    for (const auto &r : list) {
        int row = t->rowCount(); t->insertRow(row);
        auto *ip = new QTableWidgetItem(QString::fromStdString(r.ip_address));
        ip->setFont(mf); ip->setForeground(QColor("#f85149"));
        auto *reason = new QTableWidgetItem(QString::fromStdString(r.reason));
        reason->setForeground(QColor("#d29922"));
        auto *score = new QTableWidgetItem(QString::number(r.threat_score, 'f', 0));
        score->setForeground(QColor("#8c8c8c"));
        t->setItem(row, 0, ip);
        t->setItem(row, 1, reason);
        t->setItem(row, 2, score);
    }
    t->resizeColumnToContents(0);
    t->horizontalHeader()->setStretchLastSection(true);
}
