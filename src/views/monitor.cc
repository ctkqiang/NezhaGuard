#include "monitor.h"
#include "ui_monitor.h"
#include "log_model.h"
#include "gui_sink.h"
#include "detail_panel.h"
#include "radar_widget.h"
#include "theme.h"
#include "../service/database_helper.h"
#include "../core/geo_ip.h"
#include "../core/ipaddr.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStyleHints>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

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
#include <algorithm>

// SparklineWidget
void SparklineWidget::set_data(const QList<int> &d) { data_ = d; update(); }

void SparklineWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    auto bg = QColor(dark ? Theme::DkCard : Theme::LtCard);
    auto fg = QColor(dark ? Theme::Pink : Theme::PinkDeep);
    auto gr = QColor(dark ? Theme::DkBorder : Theme::LtBorder);

    p.setBrush(bg); p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 8, 8);

    if (data_.isEmpty()) {
        p.setPen(fg);
        p.setFont(QFont(QStringLiteral("Inter"), 11));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("—"));
        return;
    }

    int n = data_.size();
    int maxv = std::max(1, *std::max_element(data_.begin(), data_.end()));
    QRect r = rect().adjusted(10, 14, -10, -26);
    double w = static_cast<double>(r.width()) / (n - 1);
    double h = static_cast<double>(r.height());

    p.setPen(QPen(gr, 0.5, Qt::DotLine));
    for (int i = 1; i <= 3; ++i)
        p.drawLine(QPointF(r.left(), r.top() + h * i / 4),
                   QPointF(r.right(), r.top() + h * i / 4));

    QPainterPath fill;
    fill.moveTo(r.left(), r.bottom());
    for (int i = 0; i < n; ++i)
        fill.lineTo(QPointF(r.left() + w * i, r.bottom() - (data_[i] * h / maxv)));
    fill.lineTo(r.right(), r.bottom()); fill.closeSubpath();

    QColor fc = QColor(dark ? Theme::Pink : Theme::PinkDeep);
    fc.setAlpha(25); p.setBrush(fc); p.setPen(Qt::NoPen);
    p.drawPath(fill);

    p.setPen(QPen(fg, 1.5));
    for (int i = 1; i < n; ++i)
        p.drawLine(QPointF(r.left() + w * (i - 1), r.bottom() - (data_[i - 1] * h / maxv)),
                   QPointF(r.left() + w * i, r.bottom() - (data_[i] * h / maxv)));

    p.setPen(fg);
    p.setFont(QFont(QStringLiteral("Inter"), 9));
    p.drawText(QRect(r.left(), r.bottom() + 4, r.width(), 20),
               Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("%1 pts  max %2").arg(n).arg(maxv));
}

// Delegates
void LogDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const {
    QStyleOptionViewItem o = opt; initStyleOption(&o, idx);
    p->save();
    auto bg = dark ? (idx.row() % 2 ? QColor(Theme::DkCard) : QColor(Theme::DkBg))
                   : (idx.row() % 2 ? QColor(Theme::LtHover) : QColor(Theme::LtCard));
    if (o.state & QStyle::State_Selected) bg = dark ? QColor(Theme::DkSelected) : QColor(Theme::LtSelected);
    p->fillRect(o.rect, bg);

    QColor fg = idx.data(LogModel::ColorRole).value<QColor>();
    if (!fg.isValid()) fg = dark ? QColor(Theme::PinkLight) : QColor(Theme::PinkDeep);

    QString tm = idx.data(LogModel::TimestampRole).toString();
    QString msg = idx.data(LogModel::MessageRole).toString();
    QString lv = idx.data(LogModel::LevelRole).toString();

    QFont mf(QStringLiteral("Menlo"), 10); mf.setStyleHint(QFont::Monospace); p->setFont(mf);
    QRect r = o.rect.adjusted(10, 2, -10, -2);
    int x = r.x();

    p->setPen(dark ? QColor(Theme::DkMuted) : QColor(Theme::LtMuted));
    p->drawText(x, r.y(), r.width(), r.height(), Qt::AlignLeft | Qt::AlignVCenter, tm);
    x += p->fontMetrics().horizontalAdvance(tm) + 10;

    auto badgeW = p->fontMetrics().horizontalAdvance(lv) + 14;
    QRect bd(x, r.y() + 3, badgeW, r.height() - 6);
    p->setRenderHint(QPainter::Antialiasing);
    p->setBrush(fg); p->setPen(Qt::NoPen);
    p->drawRoundedRect(bd, 5, 5);
    p->setPen(QColor("#fff"));
    QFont bf = QApplication::font(); bf.setPointSize(8); bf.setBold(true); p->setFont(bf);
    p->drawText(bd, Qt::AlignCenter, lv);
    x += badgeW + 10;

    p->setFont(mf); p->setPen(fg);
    p->drawText(QRect(x, r.y(), r.right() - x, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                p->fontMetrics().elidedText(msg, Qt::ElideRight, r.right() - x));
    p->restore();
}

QSize LogDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const { return {200, 28}; }

void AlertDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const {
    QStyleOptionViewItem o = opt; initStyleOption(&o, idx);
    p->save();
    auto bg = dark ? (idx.row() % 2 ? QColor(Theme::DkCard) : QColor(Theme::DkBg))
                   : (idx.row() % 2 ? QColor(Theme::LtHover) : QColor(Theme::LtCard));
    if (o.state & QStyle::State_Selected) bg = dark ? QColor(Theme::DkSelected) : QColor(Theme::LtSelected);
    p->fillRect(o.rect, bg);

    QColor fg = idx.data(LogModel::ColorRole).value<QColor>();
    if (!fg.isValid()) fg = dark ? QColor(Theme::Pink) : QColor(Theme::PinkDeep);

    QString tm = idx.data(LogModel::TimestampRole).toString();
    QString msg = idx.data(LogModel::MessageRole).toString();
    QString lv = idx.data(LogModel::LevelRole).toString();

    QFont mf(QStringLiteral("Menlo"), 10); mf.setStyleHint(QFont::Monospace); p->setFont(mf);
    QRect r = o.rect.adjusted(10, 2, -10, -2);
    int x = r.x();

    p->setPen(dark ? QColor(Theme::DkMuted) : QColor(Theme::LtMuted));
    p->drawText(x, r.y(), r.width(), r.height(), Qt::AlignLeft | Qt::AlignVCenter, tm);
    x += p->fontMetrics().horizontalAdvance(tm) + 10;

    auto badgeW = p->fontMetrics().horizontalAdvance(lv) + 14;
    QRect bd(x, r.y() + 3, badgeW, r.height() - 6);
    p->setRenderHint(QPainter::Antialiasing);
    p->setBrush(fg); p->setPen(Qt::NoPen);
    p->drawRoundedRect(bd, 5, 5);
    p->setPen(QColor("#fff"));
    QFont bf = QApplication::font(); bf.setPointSize(8); bf.setBold(true); p->setFont(bf);
    p->drawText(bd, Qt::AlignCenter, lv);
    x += badgeW + 10;

    p->setFont(mf); p->setPen(fg);
    p->drawText(QRect(x, r.y(), r.right() - x, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                p->fontMetrics().elidedText(msg, Qt::ElideRight, r.right() - x));
    p->restore();
}

QSize AlertDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const { return {200, 28}; }

// monitor
monitor::monitor(QWidget *parent) : QMainWindow(parent), ui(new Ui::monitor) {
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, true);
    start_time_ = QTime::currentTime();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    dark_mode_ = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
#else
    dark_mode_ = (qApp->palette().color(QPalette::Window).lightness() < 128);
#endif

    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, &monitor::sync_theme);

    clock_timer_ = new QTimer(this);
    connect(clock_timer_, &QTimer::timeout, this, &monitor::update_clock);
    clock_timer_->start(1000);
    update_clock();

    setup_sidebar();
    start_animations();

    // shortcuts
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+F")), this, [this]() {
        ui->pages->setCurrentWidget(ui->page_logs);
        ui->sidebar->setCurrentRow(1);
        ui->log_search_box->setFocus();
    });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), this, [this]() { clear_logs(); });
    for (int k = 1; k <= 5; ++k) {
        new QShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(k)), this, [this, k]() {
            ui->sidebar->setCurrentRow(k - 1);
            ui->pages->setCurrentIndex(k - 1);
        });
    }

    sparkline_timer_ = new QTimer(this);
    connect(sparkline_timer_, &QTimer::timeout, this, &monitor::update_sparkline);
    sparkline_timer_->start(1000);
}

monitor::~monitor() { delete ui; }

// -- animations --
void monitor::start_animations() {
    auto *glow = new QGraphicsDropShadowEffect(this);
    glow->setBlurRadius(10); glow->setOffset(0, 0);
    glow->setColor(QColor(Theme::CyanLight));
    ui->app_title->setGraphicsEffect(glow);

    auto *ga = new QPropertyAnimation(glow, "blurRadius", this);
    ga->setDuration(2400); ga->setStartValue(6); ga->setEndValue(16);
    ga->setEasingCurve(QEasingCurve::InOutSine); ga->setLoopCount(-1); ga->start();

    auto *gc = new QPropertyAnimation(glow, "color", this);
    gc->setDuration(3000);
    gc->setStartValue(QColor(Theme::CyanLight));
    gc->setKeyValueAt(0.5, QColor(Theme::Cyan));
    gc->setEndValue(QColor(Theme::CyanLight));
    gc->setEasingCurve(QEasingCurve::InOutSine); gc->setLoopCount(-1); gc->start();

    for (auto *s : {ui->status_dot}) {
        auto *a = new QPropertyAnimation(s, "minimumSize", this);
        a->setDuration(1600); a->setStartValue(QSize(7, 7)); a->setEndValue(QSize(11, 11));
        a->setEasingCurve(QEasingCurve::InOutSine); a->setLoopCount(-1); a->start();
        auto *b = new QPropertyAnimation(s, "maximumSize", this);
        b->setDuration(1600); b->setStartValue(QSize(7, 7)); b->setEndValue(QSize(11, 11));
        b->setEasingCurve(QEasingCurve::InOutSine); b->setLoopCount(-1); b->start();
    }

    // card entrance
    QFrame *cards[] = {ui->card_logs, ui->card_alerts, ui->card_threats, ui->card_uptime};
    for (int i = 0; i < 4; ++i) {
        auto *fx = new QGraphicsOpacityEffect(this);
        fx->setOpacity(0.0); cards[i]->setGraphicsEffect(fx);
        auto *anim = new QPropertyAnimation(fx, "opacity", this);
        anim->setDuration(400); anim->setStartValue(0.0); anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QPropertyAnimation::finished, this, [card = cards[i]]() {
            card->setGraphicsEffect(nullptr);
        });
        QTimer::singleShot(80 + i * 60, anim, [anim]() { anim->start(); });
    }
}

// -- theme --
void monitor::sync_theme() {
    bool dk;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    dk = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
#else
    dk = (qApp->palette().color(QPalette::Window).lightness() < 128);
#endif
    if (dk != dark_mode_) { dark_mode_ = dk; apply_theme(dk); }
}

void monitor::apply_theme(bool d) {
    if (log_delegate_) log_delegate_->dark = d;
    if (alert_delegate_) alert_delegate_->dark = d;
    if (recent_delegate_) recent_delegate_->dark = d;
    if (honey_delegate_) honey_delegate_->dark = d;
    if (sparkline_widget_) sparkline_widget_->dark = d;
    if (radar_widget_) radar_widget_->dark = d;
    if (log_detail_panel_) log_detail_panel_->set_dark(d);
    if (alert_detail_panel_) alert_detail_panel_->set_dark(d);
    if (honey_detail_panel_) honey_detail_panel_->set_dark(d);

    auto B = d ? Theme::DkBg : Theme::LtBg, C = d ? Theme::DkCard : Theme::LtCard;
    auto Br = d ? Theme::DkBorder : Theme::LtBorder, T = d ? Theme::DkText : Theme::LtText;
    auto A = d ? Theme::Cyan : Theme::CyanDeep, M = d ? Theme::DkMuted : Theme::LtMuted;
    auto S = d ? Theme::DkSelected : Theme::LtSelected, H = d ? Theme::DkHover : Theme::LtHover;
    auto P = d ? Theme::Pink : Theme::PinkDeep;

    setStyleSheet(QStringLiteral(R"(
        * { font-family:"Inter","PingFang SC",sans-serif; }
        QMainWindow { background:%1; }
        #header { background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 %2,stop:1 %10); border-bottom:1px solid %3; }
        QLabel { color:%4; }
        #app_title { font-size:15px; font-weight:700; color:%5; }
        #brand_badge { background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %9,stop:1 #e0637e);
                       color:%1; border-radius:12px; font-size:9px; font-weight:700; padding:2px 8px; }
        #clock_label,#status_text { font-size:10px; color:%5; font-weight:600; }
        #status_dot { background:%5; border-radius:4px; }
        QStatusBar { background:%2; border-top:1px solid %3; font-size:10px; color:%6; }
        #sidebar { background:%2; border-right:2px solid %3; }
        #sidebar::item { color:%6; padding:12px 22px; font-size:12px; border:none; margin:1px 6px; border-radius:10px; }
        #sidebar::item:selected { background:%7; color:%5; font-weight:600; border-left:2px solid %5; }
        #sidebar::item:hover:!selected { background:%8; color:%4; }
        #dash_title,#logs_title,#alerts_title,#honey_title,#network_title,
        #recent_alerts_label,#attackers_label,#local_ip_label,#arp_label,#quarantine_label
            { font-size:12px; font-weight:600; color:%9; padding-bottom:4px; border-bottom:1px solid %3; }
        QFrame#card_logs,QFrame#card_alerts,QFrame#card_threats,QFrame#card_uptime
            { background:%2; border:1px solid %3; border-radius:14px; padding:16px 14px; border-left:4px solid %3; }
        QFrame#card_logs { border-left-color:%9; } QFrame#card_alerts { border-left-color:%5; }
        QFrame#card_threats { border-left-color:#ff6b6b; } QFrame#card_uptime { border-left-color:#7ecf8a; }
        QFrame#card_logs:hover,QFrame#card_alerts:hover,QFrame#card_threats:hover,QFrame#card_uptime:hover
            { border-color:%9; background:%10; }
        #card_logs_value,#card_alerts_value,#card_threats_value,#card_uptime_value
            { font-size:28px; font-weight:800; color:%4; }
        #card_logs_label,#card_alerts_label,#card_threats_label,#card_uptime_label
            { font-size:10px; font-weight:600; color:%6; margin-top:2px; }
        QTableView { background:%1; alternate-background-color:%10; gridline-color:%3;
                     color:%4; border:1px solid %3; border-radius:10px; font-size:11px; }
        QTableView::item:selected { background:%7; }
        QHeaderView::section { background:%2; color:%6; padding:6px 12px;
                               border:none; border-bottom:1px solid %3; font-size:10px; font-weight:600; }
        QComboBox { background:%2; color:%4; border:1px solid %3; border-radius:10px;
                    padding:5px 12px; font-size:11px; min-width:80px; }
        QComboBox:hover { border-color:%5; } QComboBox::drop-down { border:none; width:20px; }
        QComboBox QAbstractItemView { background:%2; color:%4; selection-background-color:%7;
                                      border:1px solid %3; border-radius:6px; }
        QPushButton { background:%2; color:%5; border:1px solid %3; border-radius:10px;
                      padding:6px 16px; font-size:11px; font-weight:600; }
        QPushButton:hover { background:%10; border-color:%9; color:%9; }
        QPushButton:pressed { background:%7; }
        QLineEdit { background:%2; color:%4; border:1px solid %3; border-radius:10px; padding:6px 12px; font-size:11px; }
        QLineEdit:focus { border-color:%5; }
        QTextEdit,QTableWidget { background:%1; color:%4; border:1px solid %3; border-radius:10px; font-size:11px; }
        QTableWidget::item:selected { background:%7; }
        QScrollBar:vertical { background:transparent; width:5px; }
        QScrollBar::handle:vertical { background:%3; border-radius:2px; min-height:20px; }
        QScrollBar::handle:vertical:hover { background:%5; }
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }
        #sev_crit,#sev_error,#sev_warn,#sev_info { border-radius:10px; font-family:"Menlo"; font-size:10px;
            font-weight:700; padding:4px 0; background:%2; }
        #sev_crit { color:#ff6b6b; } #sev_error { color:#ff9966; }
        #sev_warn { color:%9; } #sev_info { color:%6; }
        #qs_tor,#qs_blocked,#qs_engines,#qs_types { border-radius:10px; font-family:"Menlo"; font-size:9px;
            font-weight:700; padding:4px 6px; background:%2; color:%6; border:1px solid %3; }
    )").arg(B, C, Br, T, A, M, S, H, P, C));
}

// -- setup --
void monitor::setup_sidebar() { ui->sidebar->setCurrentRow(0); }
void monitor::update_clock() {
    ui->clock_label->setText(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd  hh:mm:ss")));
}
void monitor::setup_log_table(QTableView *v) {
    v->setShowGrid(false); v->setAlternatingRowColors(false);
    v->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->setSelectionMode(QAbstractItemView::SingleSelection);
    v->verticalHeader()->setDefaultSectionSize(28); v->verticalHeader()->hide();
    v->setMouseTracking(true);
}
void monitor::setup_network_table(QTableWidget *t) {
    t->verticalHeader()->hide(); t->horizontalHeader()->setStretchLastSection(true);
    t->setAlternatingRowColors(true); t->setSelectionBehavior(QAbstractItemView::SelectRows);
}

// -- models --
void monitor::init_models() {
    log_model_ = new LogModel(this);
    gui_sink_ = std::make_shared<GuiSink>(log_model_);

    // log page
    log_proxy_ = new QSortFilterProxyModel(this);
    log_proxy_->setSourceModel(log_model_);
    log_proxy_->setFilterRole(LogModel::LevelRole);
    log_proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    log_proxy_->setSortRole(LogModel::TimestampRole);
    log_proxy_->sort(0, Qt::DescendingOrder);

    setup_log_table(ui->log_view);
    ui->log_view->setModel(log_proxy_);
    ui->log_view->setColumnHidden(1, true);
    log_delegate_ = new LogDelegate(this); log_delegate_->dark = dark_mode_;
    ui->log_view->setItemDelegate(log_delegate_);
    ui->log_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->log_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(ui->log_view, &QTableView::clicked, this, &monitor::show_log_detail);
    connect(ui->level_filter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &monitor::apply_log_filter);

    log_search_proxy_ = new QSortFilterProxyModel(this);
    log_search_proxy_->setSourceModel(log_proxy_);
    log_search_proxy_->setFilterRole(LogModel::MessageRole);
    log_search_proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    log_search_proxy_->setDynamicSortFilter(true);
    ui->log_view->setModel(log_search_proxy_);
    connect(ui->log_search_box, &QLineEdit::textChanged, this, &monitor::log_search_changed);
    connect(ui->log_clear_btn, &QPushButton::clicked, this, &monitor::clear_logs);

    // alert page
    alert_model_ = new LogModel(this);
    alert_proxy_ = new QSortFilterProxyModel(this);
    alert_proxy_->setSourceModel(alert_model_);
    alert_proxy_->setFilterRole(LogModel::LevelRole);
    alert_proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    alert_proxy_->setSortRole(LogModel::TimestampRole);
    alert_proxy_->sort(0, Qt::DescendingOrder);

    setup_log_table(ui->alert_view);
    ui->alert_view->setModel(alert_proxy_);
    alert_delegate_ = new AlertDelegate(this); alert_delegate_->dark = dark_mode_;
    ui->alert_view->setItemDelegate(alert_delegate_);
    ui->alert_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->alert_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(ui->alert_view, &QTableView::clicked, this, &monitor::show_alert_detail);
    connect(ui->alert_severity_filter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &monitor::filter_alert_severity);

    // dashboard recent alerts
    setup_log_table(ui->recent_alerts_view);
    ui->recent_alerts_view->setModel(alert_model_);
    recent_delegate_ = new AlertDelegate(this); recent_delegate_->dark = dark_mode_;
    ui->recent_alerts_view->setItemDelegate(recent_delegate_);
    ui->recent_alerts_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->recent_alerts_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(ui->recent_alerts_view, &QTableView::clicked, this, &monitor::show_alert_detail);

    // honeypot page
    honey_model_ = new LogModel(this);
    setup_log_table(ui->honey_view);
    ui->honey_view->setModel(honey_model_);
    honey_delegate_ = new LogDelegate(this); honey_delegate_->dark = dark_mode_;
    ui->honey_view->setItemDelegate(honey_delegate_);
    ui->honey_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->honey_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(ui->honey_view, &QTableView::clicked, this, &monitor::show_honey_detail);

    // network page
    for (auto *t : {ui->local_ip_table, ui->arp_table, ui->quarantine_table})
        setup_network_table(t);

    // attackers leaderboard
    attackers_model_ = new LogModel(this);
    setup_log_table(ui->attackers_view);
    ui->attackers_view->setModel(attackers_model_);
    auto *attackersDelegate = new AlertDelegate(this);
    attackersDelegate->dark = dark_mode_;
    ui->attackers_view->setItemDelegate(attackersDelegate);
    ui->attackers_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->attackers_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(ui->refresh_network, &QPushButton::clicked, this, &monitor::refresh_network_info);
    refresh_network_info();
    auto *netTimer = new QTimer(this);
    connect(netTimer, &QTimer::timeout, this, &monitor::refresh_network_info);
    netTimer->start(30000); // 每 30s 刷新 ARP/雷达/本地接口/隔离列表

    // sparkline
    sparkline_widget_ = new SparklineWidget(); sparkline_widget_->dark = dark_mode_;
    if (ui->sparkline_frame->layout()) delete ui->sparkline_frame->layout();
    auto *vbl = new QVBoxLayout(ui->sparkline_frame); vbl->setContentsMargins(0, 0, 0, 0);
    vbl->addWidget(sparkline_widget_);

    // radar
    radar_widget_ = new RadarWidget(); radar_widget_->dark = dark_mode_;
    if (ui->radar_frame->layout()) delete ui->radar_frame->layout();
    auto *rvbl = new QVBoxLayout(ui->radar_frame); rvbl->setContentsMargins(0, 0, 0, 0);
    rvbl->addWidget(radar_widget_);

    // detail panels
    auto replace = [this](QTextEdit *old, DetailPanel *&panel, int maxH) {
        auto *pl = qobject_cast<QVBoxLayout *>(old->parentWidget()->layout());
        if (!pl) return;
        int i = pl->indexOf(old); old->hide();
        panel = new DetailPanel(this); panel->setMaximumHeight(maxH); panel->set_dark(dark_mode_);
        if (i >= 0) pl->insertWidget(i, panel);
    };
    replace(ui->log_detail, log_detail_panel_, 180);
    replace(ui->alert_detail, alert_detail_panel_, 110);
    replace(ui->honey_detail, honey_detail_panel_, 110);

    // context menus
    auto ctx = [this](const QPoint &pos, QTableView *v, QSortFilterProxyModel *proxy) {
        auto idx = v->indexAt(pos); if (!idx.isValid()) return;
        QMenu m(this);
        auto *cpy = m.addAction(QStringLiteral("复制"));
        auto *qip = m.addAction(QStringLiteral("隔离此 IP"));
        if (m.exec(v->viewport()->mapToGlobal(pos)) == cpy)
            QApplication::clipboard()->setText(proxy ? proxy->data(idx.siblingAtColumn(2), LogModel::MessageRole).toString()
                                                     : idx.data(LogModel::MessageRole).toString());
        else if (qip) {
            QRegularExpression rx(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");
            auto ip = rx.match(proxy ? proxy->data(idx.siblingAtColumn(2), LogModel::MessageRole).toString()
                                     : idx.data(LogModel::MessageRole).toString()).captured(1);
            if (!ip.isEmpty()) { Nezha::Database::DatabaseHelper::QuarantineIP(ip.toStdString(), "手动隔离", 50.0); refresh_quarantine_list(); }
        }
    };

    for (auto &[view, proxy] : {std::pair{ui->log_view, log_search_proxy_}, {ui->alert_view, alert_proxy_}}) {
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view, &QTableView::customContextMenuRequested, this, [this, view, proxy, ctx](const QPoint &p) { ctx(p, view, proxy); });
    }

    ui->honey_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->honey_view, &QTableView::customContextMenuRequested, this, [this](const QPoint &pos) {
        auto idx = ui->honey_view->indexAt(pos); if (!idx.isValid()) return;
        QMenu m(this); m.addAction(QStringLiteral("复制"));
        auto *qip = m.addAction(QStringLiteral("隔离来源 IP"));
        if (m.exec(ui->honey_view->viewport()->mapToGlobal(pos)) == qip) {
            QRegularExpression rx(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");
            auto ip = rx.match(idx.data(LogModel::MessageRole).toString()).captured(1);
            if (!ip.isEmpty()) { Nezha::Database::DatabaseHelper::QuarantineIP(ip.toStdString(), "蜜罐手动隔离", 75.0); refresh_quarantine_list(); }
        }
    });

    ui->quarantine_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->quarantine_table, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        auto *item = ui->quarantine_table->itemAt(pos); if (!item) return;
        auto ip = ui->quarantine_table->item(item->row(), 0)->text();
        QMenu m(this); m.addAction(QStringLiteral("复制 IP"));
        auto *unq = m.addAction(QStringLiteral("取消隔离"));
        if (m.exec(ui->quarantine_table->viewport()->mapToGlobal(pos)) == unq) {
            Nezha::Database::DatabaseHelper::RemoveQuarantine(ip.toStdString()); refresh_quarantine_list();
        }
    });

    apply_theme(dark_mode_);
}

// -- stats --
void monitor::update_stats(int log_count, int alert_count) {
    static int last_log = 0, last_alert = 0;
    int logRate = std::max(0, log_count - last_log);
    int alertRate = std::max(0, alert_count - last_alert);
    pkt_rate_ = logRate;
    last_log = log_count; last_alert = alert_count;
    int qc = static_cast<int>(Nezha::Database::DatabaseHelper::GetQuarantineList().size());

    auto threatLvl = alertRate > 10 ? QStringLiteral("危险") : alertRate > 3 ? QStringLiteral("警告") : QStringLiteral("正常");
    auto threatColor = alertRate > 10 ? Theme::Red : alertRate > 3 ? Theme::Orange : Theme::Green;
    ui->status_label->setText(QStringLiteral("威胁: %1 | 日志 %2 (+%3/s) | 告警 %4 (+%5/s) | 已隔离 %6")
        .arg(threatLvl).arg(log_count).arg(logRate).arg(alert_count).arg(alertRate).arg(qc));

    ui->card_logs_value->setText(QString::number(log_count));
    ui->card_logs_label->setText(QStringLiteral("日志  +%1/s").arg(logRate));
    ui->card_alerts_value->setText(QString::number(alert_count));
    ui->card_alerts_label->setText(QStringLiteral("告警  +%1/s").arg(alertRate));
    ui->card_threats_value->setText(QString::number(qc));
    ui->card_threats_label->setText(QStringLiteral("隔离  |  %1 种攻击").arg(active_types_.size()));

    int secs = std::max(1, start_time_.secsTo(QTime::currentTime()));
    int h = secs / 3600, m = (secs % 3600) / 60;
    ui->card_uptime_value->setText(QStringLiteral("%1h %2m").arg(h).arg(m));
    ui->card_uptime_label->setText(QStringLiteral("运行  |  %1 攻击者").arg(attackers_.size()));
    refresh_quickstats();
}

void monitor::append_alert(const QString &time, const QString &type, const QString &ip, int count, double score, const QString &severity) {
    if (!alert_model_) return;
    alert_model_->append(time, severity,
        QStringLiteral("%1  |  %2  x%3  %4分").arg(type, ip).arg(count).arg(static_cast<int>(score)));
    int a = alert_model_->total();
    ui->card_alerts_value->setText(QString::number(a));
    ui->alert_stats_label->setText(QStringLiteral("共 %1 条").arg(a));

    if (severity.startsWith(QStringLiteral("CRIT"))) sev_crit_++;
    else if (severity.startsWith(QStringLiteral("ERR"))) sev_error_++;
    else if (severity.startsWith(QStringLiteral("WARN"))) sev_warn_++;
    else sev_info_++;

    ui->sev_crit->setText(QStringLiteral("CRIT\n%1").arg(sev_crit_));
    ui->sev_error->setText(QStringLiteral("ERROR\n%1").arg(sev_error_));
    ui->sev_warn->setText(QStringLiteral("WARN\n%1").arg(sev_warn_));
    ui->sev_info->setText(QStringLiteral("INFO\n%1").arg(sev_info_));

    // track active attack types
    active_types_.insert(type);
    QStringList types;
    for (const auto &t : active_types_) {
        auto color = sev_crit_ > 0 ? Theme::PinkDeep : Theme::Pink;
        types.append(QStringLiteral("<span style='color:%1;background:%2;border-radius:8px;padding:2px 8px;margin:1px;font-size:10px;font-weight:600;'>%3</span>")
            .arg(Theme::White, Theme::DkCard, t));
    }
    ui->threat_activity->setText(types.isEmpty() ? QString() : types.join(QStringLiteral(" ")));
    ui->threat_activity->setTextFormat(Qt::RichText);
}

void monitor::append_honeypot(const QString &time, const QString &src_ip, uint16_t sport, uint16_t dport, const QString &service) {
    if (!honey_model_) return;
    honey_model_->append(time, QStringLiteral("INFO"),
        QStringLiteral("%1:%2 → :%3 [%4]").arg(src_ip).arg(sport).arg(dport).arg(service));
    ui->honey_stats_label->setText(QStringLiteral("共 %1 次连接").arg(honey_model_->total()));
}

void monitor::record_attacker(const QString &ip, double score, const QString &type) {
    auto &a = attackers_[ip]; a.score = std::max(a.score, score); a.count++;
    if (!type.isEmpty()) a.type = type;

    QList<std::pair<QString, Attacker>> sorted;
    for (auto it = attackers_.begin(); it != attackers_.end(); ++it)
        sorted.append({it.key(), it.value()});
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
        return a.second.score * a.second.count > b.second.score * b.second.count;
    });

    attackers_model_->clear();
    int rank = 0;
    for (const auto &[ip, a] : sorted) {
        if (++rank > 10) break;
        attackers_model_->append(
            QStringLiteral("#%1").arg(rank),
            a.score > 80 ? QStringLiteral("CRIT") : a.score > 50 ? QStringLiteral("WARN") : QStringLiteral("INFO"),
            QStringLiteral("%1  |  %2分  x%3  [%4]").arg(ip).arg(static_cast<int>(a.score)).arg(a.count).arg(a.type));
    }
}

// -- filters --
void monitor::apply_log_filter(int idx) { if (log_proxy_) log_proxy_->setFilterFixedString(idx == 0 ? QString() : ui->level_filter->currentText()); }
void monitor::filter_alert_severity(int idx) { if (alert_proxy_) alert_proxy_->setFilterFixedString(idx == 0 ? QString() : ui->alert_severity_filter->currentText()); }
void monitor::log_search_changed(const QString &t) { if (log_search_proxy_) log_search_proxy_->setFilterFixedString(t); }

// -- detail --
void monitor::show_log_detail(const QModelIndex &idx) {
    if (!idx.isValid() || !log_detail_panel_) return;
    auto tm = idx.data(LogModel::TimestampRole).toString();
    auto lv = idx.data(LogModel::LevelRole).toString();
    auto msg = idx.data(LogModel::MessageRole).toString();
    QRegularExpression rx(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");
    QStringList ips; auto it = rx.globalMatch(msg);
    while (it.hasNext()) { auto ip = it.next().captured(1); if (!ips.contains(ip)) ips.append(ip); }
    log_detail_panel_->show_log(tm, lv, msg, ips);
    if (!ips.isEmpty()) {
        auto ips2 = ips;
        auto _f = QtConcurrent::run([this, ips2]() {
            for (const auto &ip : ips2) {
                auto geo = Nezha::Core::GeoIP::lookup(ip.toStdString());
                auto host = Nezha::IPAddress::ipaddr::ResolveHostname(ip.toStdString());
                QMetaObject::invokeMethod(this, [this, ip, host, geo]() {
                    if (log_detail_panel_) log_detail_panel_->add_geo_info(ip, QString::fromStdString(host), geo);
                }, Qt::QueuedConnection);
            }
        });
    }
}

void monitor::show_alert_detail(const QModelIndex &idx) {
    if (!idx.isValid() || !alert_detail_panel_) return;
    alert_detail_panel_->show_alert(
        idx.data(LogModel::TimestampRole).toString(),
        idx.data(LogModel::LevelRole).toString(),
        idx.data(LogModel::MessageRole).toString());
}

void monitor::show_honey_detail(const QModelIndex &idx) {
    if (!idx.isValid() || !honey_detail_panel_) return;
    honey_detail_panel_->show_honeypot(
        idx.data(LogModel::TimestampRole).toString(),
        idx.data(LogModel::MessageRole).toString());
}

// -- network --
void monitor::refresh_local_ips() {
    auto *t = ui->local_ip_table; t->setRowCount(0);
    t->setHorizontalHeaderLabels({QStringLiteral("接口"), QStringLiteral("IP 地址")});
    ifaddrs *ifap = nullptr; if (getifaddrs(&ifap) != 0) return;
    QFont mf(QStringLiteral("Menlo"), 10); mf.setStyleHint(QFont::Monospace);
    for (auto *ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || (ifa->ifa_flags & IFF_LOOPBACK)) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        char b[INET6_ADDRSTRLEN] = {0};
        auto *s = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
        const char *ip = inet_ntop(AF_INET, &s->sin_addr, b, sizeof(b));
        if (!ip) continue;
        int r = t->rowCount(); t->insertRow(r);
        auto *n = new QTableWidgetItem(ifa->ifa_name); n->setForeground(QColor(Theme::Pink));
        auto *i = new QTableWidgetItem(ip); i->setFont(mf); i->setForeground(QColor(Theme::Green));
        t->setItem(r, 0, n); t->setItem(r, 1, i);
    }
    freeifaddrs(ifap);
    t->resizeColumnToContents(0); t->horizontalHeader()->setStretchLastSection(true);
}

void monitor::refresh_arp_table() {
    auto *t = ui->arp_table; t->setRowCount(0);
    t->setHorizontalHeaderLabels({QStringLiteral("IP 地址"), QStringLiteral("MAC 地址")});
    std::set<std::string> local;
    ifaddrs *ifap = nullptr;
    if (getifaddrs(&ifap) == 0) {
        for (auto *ifa = ifap; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            char b[INET_ADDRSTRLEN] = {0};
            auto *s = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
            inet_ntop(AF_INET, &s->sin_addr, b, sizeof(b)); local.insert(b);
        }
        freeifaddrs(ifap);
    }
    int mib[6] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO};
    std::size_t n = 0;
    if (sysctl(mib, 6, nullptr, &n, nullptr, 0) != 0 || n == 0) return;
    std::vector<char> buf(n * 2); n = buf.size();
    if (sysctl(mib, 6, buf.data(), &n, nullptr, 0) != 0) return;
    std::set<std::string> seen;
    QFont mf(QStringLiteral("Menlo"), 10); mf.setStyleHint(QFont::Monospace);
    for (char *p = buf.data(); p < buf.data() + n;) {
        auto *rtm = reinterpret_cast<rt_msghdr *>(p);
        if (rtm->rtm_version != RTM_VERSION) break;
        if (!(rtm->rtm_flags & RTF_LLINFO) || (rtm->rtm_flags & (RTF_LOCAL | RTF_BROADCAST | RTF_MULTICAST))) { p += rtm->rtm_msglen; continue; }
        auto *sa = reinterpret_cast<sockaddr *>(rtm + 1);
        int addrs = rtm->rtm_addrs;
        char ipb[INET_ADDRSTRLEN] = {0}, mb[18] = {0};
        const char *ips = nullptr; bool hm = false;
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
                    snprintf(mb, sizeof(mb), "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]); hm = true;
                }
            }
            sa = reinterpret_cast<sockaddr *>(reinterpret_cast<char *>(sa) + sl);
        }
        if (!ips || !hm) { p += rtm->rtm_msglen; continue; }
        if (mb[0] == 'f' && mb[1] == 'f') { p += rtm->rtm_msglen; continue; }
        if ((mb[0] == '0' && mb[1] == '0') || (mb[1] & 1)) { p += rtm->rtm_msglen; continue; }
        if (local.count(ips)) { p += rtm->rtm_msglen; continue; }
        if (!seen.insert(std::string(ips) + "@" + mb).second) { p += rtm->rtm_msglen; continue; }
        int row = t->rowCount(); t->insertRow(row);
        auto *ipx = new QTableWidgetItem(ips); ipx->setFont(mf); ipx->setForeground(QColor(Theme::Green));
        auto *mx = new QTableWidgetItem(mb); mx->setFont(mf); mx->setForeground(QColor(Theme::DkMuted));
        t->setItem(row, 0, ipx); t->setItem(row, 1, mx);
        p += rtm->rtm_msglen;
    }
    t->resizeColumnToContents(0); t->horizontalHeader()->setStretchLastSection(true);

    // populate radar
    if (radar_widget_) {
        QVector<RadarDevice> devices;
        for (int r = 0; r < t->rowCount(); ++r) {
            RadarDevice d;
            d.ip = t->item(r, 0) ? t->item(r, 0)->text() : QString();
            d.mac = t->item(r, 1) ? t->item(r, 1)->text() : QString();
            if (!d.ip.isEmpty()) devices.append(d);
        }
        radar_widget_->set_devices(devices);
    }
}

void monitor::refresh_network_info() { refresh_local_ips(); refresh_arp_table(); refresh_quarantine_list(); }

void monitor::refresh_quarantine_list() {
    auto list = Nezha::Database::DatabaseHelper::GetQuarantineList();
    ui->card_threats_value->setText(QString::number(static_cast<int>(list.size())));
    auto *t = ui->quarantine_table; t->setRowCount(0);
    t->setHorizontalHeaderLabels({QStringLiteral("IP 地址"), QStringLiteral("隔离原因"), QStringLiteral("威胁评分")});
    QFont mf(QStringLiteral("Menlo"), 10); mf.setStyleHint(QFont::Monospace);
    for (const auto &r : list) {
        int row = t->rowCount(); t->insertRow(row);
        auto *i1 = new QTableWidgetItem(QString::fromStdString(r.ip_address)); i1->setFont(mf); i1->setForeground(QColor(Theme::Red));
        auto *i2 = new QTableWidgetItem(QString::fromStdString(r.reason)); i2->setForeground(QColor(Theme::Pink));
        auto *i3 = new QTableWidgetItem(QString::number(r.threat_score, 'f', 0)); i3->setForeground(QColor(Theme::DkMuted));
        t->setItem(row, 0, i1); t->setItem(row, 1, i2); t->setItem(row, 2, i3);
    }
    t->resizeColumnToContents(0); t->horizontalHeader()->setStretchLastSection(true);
}

// -- misc --
void monitor::clear_logs() {
    sev_crit_ = sev_error_ = sev_warn_ = sev_info_ = 0;
    active_types_.clear();
    if (log_model_) log_model_->clear();
    if (alert_model_) alert_model_->clear();
    if (honey_model_) honey_model_->clear();
    ui->card_logs_value->setText(QStringLiteral("0"));
    ui->card_alerts_value->setText(QStringLiteral("0"));
    for (auto *p : {log_detail_panel_, alert_detail_panel_, honey_detail_panel_}) if (p) p->clear();
}

void monitor::update_sparkline() {
    int cur = log_model_ ? log_model_->total() : 0;
    static int last = 0;
    int delta = std::max(0, cur - last); last = cur;
    sparkline_data_.append(delta);
    if (sparkline_data_.size() > 60) sparkline_data_.removeFirst();
    if (sparkline_widget_) sparkline_widget_->set_data(sparkline_data_);
}

void monitor::refresh_quickstats() {
    int qc = static_cast<int>(Nezha::Database::DatabaseHelper::GetQuarantineList().size());
    auto set_qs = [](QLabel *l, const QString &label, int n, const QString &color) {
        l->setText(QStringLiteral("%1\n%2").arg(label).arg(n));
    };
    set_qs(ui->qs_tor, QStringLiteral("攻击者"), attackers_.size(), Theme::Pink);
    set_qs(ui->qs_blocked, QStringLiteral("已隔离"), qc, Theme::PinkDeep);
    set_qs(ui->qs_engines, QStringLiteral("引擎数"), 3, Theme::Green);
    set_qs(ui->qs_types, QStringLiteral("攻击类型"), active_types_.size(), Theme::PinkLight);
}
