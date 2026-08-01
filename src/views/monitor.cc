#include "monitor.h"
#include "ui_monitor.h"
#include "log_model.h"
#include "gui_sink.h"
#include "../service/database_helper.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QRegularExpression>
#include <QShortcut>
#include <QtConcurrent>
#include "../core/geo_ip.h"
#include "../core/ipaddr.h"
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
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

class SparklineWidget : public QFrame {
public:
    using QFrame::QFrame;
    void set_data(const QList<int> &d) { data_ = d; update(); }
    bool dark = true;
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor bg = dark ? QColor("#0d2129") : QColor("#ffffff");
        QColor ln = dark ? QColor("#39c5bb") : QColor("#00bcd4");
        QColor fg = dark ? QColor("#4dd0e1") : QColor("#00838f");
        QColor gr = dark ? QColor("#1a3a44") : QColor("#b2ebf2");
        p.setBrush(bg);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect(), 10, 10);
        if (data_.isEmpty()) {
            p.setPen(fg);
            p.setFont(QFont(QStringLiteral("PingFang SC"), 11));
            p.drawText(rect(), Qt::AlignCenter, QStringLiteral("等待数据..."));
            return;
        }
        int n = data_.size();
        int maxv = *std::max_element(data_.begin(), data_.end());
        if (maxv == 0) maxv = 1;
        QRect r = rect().adjusted(12, 16, -12, -28);
        double w = static_cast<double>(r.width()) / (n - 1);
        double h = static_cast<double>(r.height());
        // grid lines
        p.setPen(QPen(gr, 0.5, Qt::DotLine));
        for (int i = 1; i <= 3; ++i)
            p.drawLine(QPointF(r.left(), r.top() + h * i / 4), QPointF(r.right(), r.top() + h * i / 4));
        // fill area under curve
        QPainterPath fill;
        fill.moveTo(r.left(), r.bottom());
        for (int i = 0; i < n; ++i)
            fill.lineTo(QPointF(r.left() + w * i, r.bottom() - (data_[i] * h / maxv)));
        fill.lineTo(r.right(), r.bottom());
        fill.closeSubpath();
        QColor fc = ln; fc.setAlpha(25);
        p.setPen(Qt::NoPen);
        p.setBrush(fc);
        p.drawPath(fill);
        // line
        QPen pen(ln, 1.8);
        p.setPen(pen);
        for (int i = 1; i < n; ++i) {
            p.drawLine(QPointF(r.left() + w * (i - 1), r.bottom() - (data_[i - 1] * h / maxv)),
                       QPointF(r.left() + w * i,     r.bottom() - (data_[i] * h / maxv)));
        }
        // dots
        for (int i = 1; i < n; i += 3) {
            p.setBrush(ln);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(r.left() + w * i, r.bottom() - (data_[i] * h / maxv)), 2.5, 2.5);
        }
        // label
        p.setPen(fg);
        p.setFont(QFont(QStringLiteral("PingFang SC"), 9));
        p.drawText(QRect(r.left(), r.bottom() + 4, r.width(), 20), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("事件速率 (%1s)").arg(n));
        p.drawText(QRect(r.left(), r.bottom() + 4, r.width(), 20), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("峰值: %1").arg(maxv));
    }
private:
    QList<int> data_;
};

class LogDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    bool dark = true;

    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        QStyleOptionViewItem o = opt;
        initStyleOption(&o, idx);
        p->save();
        QColor bg = dark ? (idx.row() % 2 ? QColor("#0d2129") : QColor("#0a1922"))
                         : (idx.row() % 2 ? QColor("#f0fdff") : QColor("#ffffff"));
        QColor fg = idx.data(LogModel::ColorRole).value<QColor>();
        if (!fg.isValid()) fg = dark ? QColor("#b2ebf2") : QColor("#006064");
        if (o.state & QStyle::State_Selected) { bg = dark ? QColor("#132e38") : QColor("#e0f7fa"); }
        p->fillRect(o.rect, bg);
        QString tm = idx.data(LogModel::TimestampRole).toString();
        QString msg = idx.data(LogModel::MessageRole).toString();
        QString lv = idx.data(LogModel::LevelRole).toString();
        QFont mf(QStringLiteral("Menlo"), 10);
        mf.setStyleHint(QFont::Monospace);
        p->setFont(mf);
        QRect r = o.rect.adjusted(10, 2, -10, -2);
        int x = r.x();
        p->setPen(dark ? QColor("#4dd0e1") : QColor("#0097a7"));
        p->drawText(x, r.y(), r.width(), r.height(), Qt::AlignLeft | Qt::AlignVCenter, tm);
        x += p->fontMetrics().horizontalAdvance(tm) + 10;
        QRect bd(x, r.y() + 2, 56, r.height() - 4);
        p->setRenderHint(QPainter::Antialiasing);
        p->setBrush(fg.darker(dark ? 140 : 110));
        p->setPen(Qt::NoPen);
        p->drawRoundedRect(bd, 3, 3);
        p->setPen(dark ? fg.lighter(130) : QColor("#ffffff"));
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
        QColor bg = dark ? (idx.row() % 2 ? QColor("#0d2129") : QColor("#0a1922"))
                         : (idx.row() % 2 ? QColor("#f0fdff") : QColor("#ffffff"));
        QColor fg = idx.data(LogModel::ColorRole).value<QColor>();
        if (!fg.isValid()) fg = dark ? QColor("#b2ebf2") : QColor("#006064");
        if (o.state & QStyle::State_Selected) { bg = dark ? QColor("#132e38") : QColor("#e0f7fa"); }
        p->fillRect(o.rect, bg);
        QString tm = idx.data(LogModel::TimestampRole).toString();
        QString msg = idx.data(LogModel::MessageRole).toString();
        QString lv = idx.data(LogModel::LevelRole).toString();
        QFont mf(QStringLiteral("Menlo"), 10);
        mf.setStyleHint(QFont::Monospace);
        p->setFont(mf);
        QRect r = o.rect.adjusted(10, 2, -10, -2);
        int x = r.x();
        p->setPen(dark ? QColor("#4dd0e1") : QColor("#0097a7"));
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

    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, &monitor::sync_theme);

    clock_timer_ = new QTimer(this);
    connect(clock_timer_, &QTimer::timeout, this, &monitor::update_clock);
    clock_timer_->start(1000);
    update_clock();

    setup_sidebar();

    auto *pulse = new QPropertyAnimation(ui->status_dot, "minimumSize", this);
    pulse->setDuration(1200);
    pulse->setStartValue(QSize(8, 8));
    pulse->setEndValue(QSize(12, 12));
    pulse->setEasingCurve(QEasingCurve::InOutSine);
    pulse->setLoopCount(-1);
    auto *pulse2 = new QPropertyAnimation(ui->status_dot, "maximumSize", this);
    pulse2->setDuration(1200);
    pulse2->setStartValue(QSize(8, 8));
    pulse2->setEndValue(QSize(12, 12));
    pulse2->setEasingCurve(QEasingCurve::InOutSine);
    pulse2->setLoopCount(-1);
    pulse->start();
    pulse2->start();

    // keyboard shortcuts
    auto *fcs = new QShortcut(QKeySequence(QStringLiteral("Ctrl+F")), this);
    connect(fcs, &QShortcut::activated, this, [this]() {
        ui->pages->setCurrentWidget(ui->page_logs);
        ui->sidebar->setCurrentRow(1);
        ui->log_search_box->setFocus();
    });
    auto *cls = new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), this);
    connect(cls, &QShortcut::activated, this, &monitor::clear_logs);
    for (int k = 1; k <= 5; ++k) {
        auto *sc = new QShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(k)), this);
        connect(sc, &QShortcut::activated, this, [this, k]() {
            ui->sidebar->setCurrentRow(k - 1);
            ui->pages->setCurrentIndex(k - 1);
        });
    }

    // sparkline timer
    sparkline_timer_ = new QTimer(this);
    connect(sparkline_timer_, &QTimer::timeout, this, &monitor::update_sparkline);
    sparkline_timer_->start(1000);

    /* 标题光晕呼吸动画: 哪吒网络安全 SIEM */
    auto *glow = new QGraphicsDropShadowEffect(this);
    glow->setBlurRadius(12);
    glow->setOffset(0, 0);
    glow->setColor(QColor("#39c5bb"));
    ui->app_title->setGraphicsEffect(glow);
    auto *glow_anim = new QPropertyAnimation(glow, "blurRadius", this);
    glow_anim->setDuration(1800);
    glow_anim->setStartValue(6);
    glow_anim->setEndValue(20);
    glow_anim->setEasingCurve(QEasingCurve::InOutSine);
    glow_anim->setLoopCount(-1);
    glow_anim->start();
    auto *glow_color = new QPropertyAnimation(glow, "color", this);
    glow_color->setDuration(2200);
    glow_color->setStartValue(QColor("#4dd0e1"));
    glow_color->setKeyValueAt(0.33, QColor("#39c5bb"));
    glow_color->setKeyValueAt(0.66, QColor("#00bcd4"));
    glow_color->setEndValue(QColor("#4dd0e1"));
    glow_color->setEasingCurve(QEasingCurve::InOutSine);
    glow_color->setLoopCount(-1);
    glow_color->start();
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
    if (log_delegate_) static_cast<LogDelegate *>(log_delegate_)->dark = dark;
    if (alert_delegate_) static_cast<AlertDelegate *>(alert_delegate_)->dark = dark;
    if (recent_delegate_) static_cast<AlertDelegate *>(recent_delegate_)->dark = dark;
    if (honey_delegate_) static_cast<LogDelegate *>(honey_delegate_)->dark = dark;
    if (sparkline_widget_) static_cast<SparklineWidget *>(sparkline_widget_)->dark = dark;

    if (dark) {
        setStyleSheet(QStringLiteral(R"(
            * { font-family:"PingFang SC","Microsoft YaHei",sans-serif; }
            QMainWindow { background:#0a1922; }
            #header { background:#0d2129; border-bottom:2px solid #00bcd4; }
            QLabel { color:#b2ebf2; }
            #app_title { font-size:16px; font-weight:800; color:#4dd0e1; letter-spacing:1px; }
            #brand_badge { background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #00bcd4,stop:1 #39c5bb); color:#0a1922; border-radius:13px; font-size:10px; font-weight:800; padding:1px 6px; }
            #clock_label { font-size:11px; color:#80deea; }
            #status_dot { background:#39c5bb; border-radius:4px; }
            #status_text { font-size:11px; color:#39c5bb; font-weight:bold; }
            QStatusBar { background:#0d2129; border-top:1px solid #1a3a44; font-size:11px; }
            #sidebar { background:#0d2129; border-right:1px solid #1a3a44; }
            #sidebar::item { color:#4dd0e1; padding:12px 24px; font-size:13px; border:none; margin:1px 8px; border-radius:8px; }
            #sidebar::item:selected { background:#132e38; color:#39c5bb; font-weight:700; border-left:3px solid #39c5bb; }
            #sidebar::item:hover:!selected { background:#101f28; color:#b2ebf2; }
            #recent_alerts_label, #dash_title, #logs_title, #alerts_title, #honey_title, #network_title,
            #local_ip_label, #arp_label, #quarantine_label { font-size:13px; font-weight:700; color:#4dd0e1; padding-bottom:4px; border-bottom:1px solid #1a3a44; }
            QFrame#card_logs, QFrame#card_alerts, QFrame#card_threats, QFrame#card_uptime {
                background:#0d2129; border:1px solid #1a3a44; border-radius:14px; padding:20px 16px; }
            QFrame#card_logs:hover, QFrame#card_alerts:hover, QFrame#card_threats:hover, QFrame#card_uptime:hover {
                border-color:#39c5bb; background:#0f242d; }
            #card_logs_value, #card_alerts_value, #card_threats_value, #card_uptime_value {
                font-size:32px; font-weight:800; color:#4dd0e1; }
            #card_logs_label, #card_alerts_label, #card_threats_label, #card_uptime_label {
                font-size:11px; color:#80deea; margin-top:2px; }
            QTableView { background:#0a1922; alternate-background-color:#0d2129; gridline-color:#1a3a44;
                         color:#b2ebf2; border:1px solid #1a3a44; border-radius:10px; font-size:11px; }
            QTableView::item:selected { background:#132e38; }
            QHeaderView::section { background:#0d2129; color:#4dd0e1; padding:7px 14px;
                                   border:none; border-bottom:2px solid #1a3a44; font-size:11px; font-weight:700; }
            QComboBox { background:#0d2129; color:#b2ebf2; border:1px solid #1a3a44; border-radius:8px;
                        padding:6px 14px; font-size:12px; min-width:90px; }
            QComboBox:hover { border-color:#39c5bb; }
            QComboBox::drop-down { border:none; width:24px; }
            QComboBox QAbstractItemView { background:#132e38; color:#b2ebf2; selection-background-color:#1a3a44; border:1px solid #1a3a44; border-radius:6px; }
            QPushButton { background:#132e38; color:#39c5bb; border:1px solid #1a3a44; border-radius:8px; padding:7px 18px; font-size:12px; font-weight:600; }
            QPushButton:hover { background:#1a3a44; border-color:#39c5bb; color:#4dd0e1; }
            QPushButton:pressed { background:#0d2129; }
            QTableWidget { background:#0a1922; gridline-color:#1a3a44; color:#b2ebf2; border:1px solid #1a3a44; border-radius:10px; font-size:11px; }
            QTableWidget::item:selected { background:#132e38; }
            QTextEdit { background:#0a1922; color:#4dd0e1; border:1px solid #1a3a44; border-radius:10px; font-size:11px; padding:8px; }
            QScrollBar:vertical { background:transparent; width:6px; margin:2px; }
            QScrollBar::handle:vertical { background:#1a3a44; border-radius:3px; min-height:24px; }
            QScrollBar::handle:vertical:hover { background:#2a4a54; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        )"));
    } else {
        setStyleSheet(QStringLiteral(R"(
            * { font-family:"PingFang SC","Microsoft YaHei",sans-serif; }
            QMainWindow { background:#e0f7fa; }
            #header { background:#ffffff; border-bottom:2px solid #00bcd4; }
            QLabel { color:#006064; }
            #app_title { font-size:16px; font-weight:800; color:#00838f; letter-spacing:1px; }
            #brand_badge { background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #00bcd4,stop:1 #39c5bb); color:#ffffff; border-radius:13px; font-size:10px; font-weight:800; padding:1px 6px; }
            #clock_label { font-size:11px; color:#00bcd4; }
            #status_dot { background:#39c5bb; border-radius:4px; }
            #status_text { font-size:11px; color:#39c5bb; font-weight:bold; }
            QStatusBar { background:#ffffff; border-top:1px solid #b2ebf2; font-size:11px; }
            #sidebar { background:#ffffff; border-right:1px solid #b2ebf2; }
            #sidebar::item { color:#0097a7; padding:12px 24px; font-size:13px; border:none; margin:1px 8px; border-radius:8px; }
            #sidebar::item:selected { background:#e0f7fa; color:#00bcd4; font-weight:700; border-left:3px solid #00bcd4; }
            #sidebar::item:hover:!selected { background:#f0fdff; color:#00838f; }
            #recent_alerts_label, #dash_title, #logs_title, #alerts_title, #honey_title, #network_title,
            #local_ip_label, #arp_label, #quarantine_label { font-size:13px; font-weight:700; color:#00838f; padding-bottom:4px; border-bottom:1px solid #b2ebf2; }
            QFrame#card_logs, QFrame#card_alerts, QFrame#card_threats, QFrame#card_uptime {
                background:#ffffff; border:1px solid #b2ebf2; border-radius:14px; padding:20px 16px; }
            QFrame#card_logs:hover, QFrame#card_alerts:hover, QFrame#card_threats:hover, QFrame#card_uptime:hover {
                border-color:#00bcd4; background:#f0fdff; }
            #card_logs_value, #card_alerts_value, #card_threats_value, #card_uptime_value {
                font-size:32px; font-weight:800; color:#00838f; }
            #card_logs_label, #card_alerts_label, #card_threats_label, #card_uptime_label {
                font-size:11px; color:#4dd0e1; margin-top:2px; }
            QTableView { background:#ffffff; alternate-background-color:#f0fdff; gridline-color:#b2ebf2;
                         color:#006064; border:1px solid #b2ebf2; border-radius:10px; font-size:11px; }
            QTableView::item:selected { background:#e0f7fa; color:#00838f; }
            QHeaderView::section { background:#f0fdff; color:#0097a7; padding:7px 14px;
                                   border:none; border-bottom:2px solid #b2ebf2; font-size:11px; font-weight:700; }
            QComboBox { background:#ffffff; color:#006064; border:1px solid #b2ebf2; border-radius:8px;
                        padding:6px 14px; font-size:12px; min-width:90px; }
            QComboBox:hover { border-color:#00bcd4; }
            QComboBox::drop-down { border:none; width:24px; }
            QComboBox QAbstractItemView { background:#ffffff; color:#006064; selection-background-color:#e0f7fa; border:1px solid #b2ebf2; border-radius:6px; }
            QPushButton { background:#ffffff; color:#00bcd4; border:1px solid #4dd0e1; border-radius:8px; padding:7px 18px; font-size:12px; font-weight:600; }
            QPushButton:hover { background:#e0f7fa; color:#0097a7; border-color:#00bcd4; }
            QPushButton:pressed { background:#b2ebf2; }
            QTableWidget { background:#ffffff; gridline-color:#b2ebf2; color:#006064; border:1px solid #b2ebf2; border-radius:10px; font-size:11px; }
            QTableWidget::item:selected { background:#e0f7fa; }
            QTextEdit { background:#ffffff; color:#00838f; border:1px solid #b2ebf2; border-radius:10px; font-size:11px; padding:8px; }
            QScrollBar:vertical { background:transparent; width:6px; margin:2px; }
            QScrollBar::handle:vertical { background:#b2ebf2; border-radius:3px; min-height:24px; }
            QScrollBar::handle:vertical:hover { background:#80deea; }
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
    connect(ui->log_view, &QTableView::clicked, this, &monitor::show_log_detail);

    connect(ui->level_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &monitor::apply_log_filter);

    // search proxy for logs (text search on top of level filter)
    log_search_proxy_ = new QSortFilterProxyModel(this);
    log_search_proxy_->setSourceModel(log_proxy_);
    log_search_proxy_->setFilterRole(LogModel::MessageRole);
    log_search_proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    ui->log_view->setModel(log_search_proxy_);
    connect(ui->log_search_box, &QLineEdit::textChanged, this, &monitor::on_log_search_changed);
    connect(ui->log_clear_btn, &QPushButton::clicked, this, &monitor::clear_logs);

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

    // replace sparkline_frame placeholder with SparklineWidget
    sparkline_widget_ = new SparklineWidget();
    static_cast<SparklineWidget *>(sparkline_widget_)->dark = dark_mode_;
    sparkline_widget_->setObjectName(QStringLiteral("sparkline_widget"));
    if (ui->sparkline_frame->layout()) {
        delete ui->sparkline_frame->layout();
    }
    auto *vbl = new QVBoxLayout(ui->sparkline_frame);
    vbl->setContentsMargins(0, 0, 0, 0);
    vbl->addWidget(sparkline_widget_);

    // context menus for tables
    auto build_log_ctx = [this](const QPoint &pos, QTableView *view, QSortFilterProxyModel *proxy) {
        QModelIndex idx = view->indexAt(pos);
        if (!idx.isValid()) return;
        QMenu menu(this);
        QAction *cpy = menu.addAction(QStringLiteral("复制内容"));
        QAction *geo = menu.addAction(QStringLiteral("查询 GeoIP"));
        QAction *qip = menu.addAction(QStringLiteral("隔离此 IP"));
        QAction *act = menu.exec(view->viewport()->mapToGlobal(pos));
        if (!act) return;
        QString msg = proxy ? proxy->data(idx.siblingAtColumn(2), LogModel::MessageRole).toString()
                         : idx.data(LogModel::MessageRole).toString();
        QRegularExpression ipr(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");
        QString ip = ipr.match(msg).captured(1);
        if (act == cpy) {
            QApplication::clipboard()->setText(msg);
        } else if (act == geo && !ip.isEmpty()) {
            show_log_detail(idx);
        } else if (act == qip && !ip.isEmpty()) {
            Nezha::Database::DatabaseHelper::QuarantineIP(ip.toStdString(), QStringLiteral("手动隔离").toStdString(), 50.0);
            refresh_quarantine_list();
        }
    };

    ui->log_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->log_view, &QTableView::customContextMenuRequested, this,
            [this, build_log_ctx](const QPoint &p) { build_log_ctx(p, ui->log_view, log_search_proxy_); });

    ui->alert_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->alert_view, &QTableView::customContextMenuRequested, this,
            [this, build_log_ctx](const QPoint &p) { build_log_ctx(p, ui->alert_view, alert_proxy_); });

    ui->honey_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->honey_view, &QTableView::customContextMenuRequested, this,
            [this](const QPoint &pos) {
        QModelIndex idx = ui->honey_view->indexAt(pos);
        if (!idx.isValid()) return;
        QMenu menu(this);
        QAction *cpy = menu.addAction(QStringLiteral("复制内容"));
        QAction *qip = menu.addAction(QStringLiteral("隔离来源 IP"));
        QAction *act = menu.exec(ui->honey_view->viewport()->mapToGlobal(pos));
        if (!act) return;
        QString msg = idx.data(LogModel::MessageRole).toString();
        if (act == cpy) {
            QApplication::clipboard()->setText(msg);
        } else if (act == qip) {
            QRegularExpression ipr(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");
            QString ip = ipr.match(msg).captured(1);
            if (!ip.isEmpty()) {
                Nezha::Database::DatabaseHelper::QuarantineIP(ip.toStdString(), QStringLiteral("蜜罐手动隔离").toStdString(), 75.0);
                refresh_quarantine_list();
            }
        }
    });

    // network table context menu - quarantine IP on right click
    ui->quarantine_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->quarantine_table, &QTableWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
        auto *item = ui->quarantine_table->itemAt(pos);
        if (!item) return;
        QString ip = ui->quarantine_table->item(item->row(), 0)->text();
        QMenu menu(this);
        QAction *cpy = menu.addAction(QStringLiteral("复制 IP"));
        QAction *unq = menu.addAction(QStringLiteral("取消隔离"));
        QAction *act = menu.exec(ui->quarantine_table->viewport()->mapToGlobal(pos));
        if (act == cpy) QApplication::clipboard()->setText(ip);
        else if (act == unq) {
            Nezha::Database::DatabaseHelper::RemoveQuarantine(ip.toStdString());
            refresh_quarantine_list();
        }
    });

    apply_theme(dark_mode_);
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

void monitor::show_log_detail(const QModelIndex &idx) {
    if (!idx.isValid()) return;
    QString tm = idx.data(LogModel::TimestampRole).toString();
    QString lv = idx.data(LogModel::LevelRole).toString();
    QString msg = idx.data(LogModel::MessageRole).toString();

    // 提取 IP 地址
    QRegularExpression ip_re(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");
    QStringList ips;
    auto it = ip_re.globalMatch(msg);
    while (it.hasNext()) {
        auto m = it.next();
        QString ip = m.captured(1);
        if (!ips.contains(ip)) ips.append(ip);
    }

    QString detail;
    detail += QStringLiteral("══════ 日志详情 ══════\n\n");
    detail += QStringLiteral("时间: %1\n").arg(tm);
    detail += QStringLiteral("级别: %1\n").arg(lv);
    detail += QStringLiteral("内容: %1\n\n").arg(msg);

    // Hex dump of the message
    QByteArray raw = msg.toUtf8();
    detail += QStringLiteral("──── Hex Dump ────\n");
    for (int i = 0; i < raw.size(); i += 16) {
        detail += QStringLiteral("%1  ").arg(i, 4, 16, QChar('0'));
        for (int j = 0; j < 16; ++j) {
            if (i + j < raw.size())
                detail += QStringLiteral("%1 ").arg(static_cast<unsigned char>(raw[i + j]), 2, 16, QChar('0'));
            else
                detail += QStringLiteral("   ");
        }
        detail += QStringLiteral(" ");
        for (int j = 0; j < 16 && (i + j) < raw.size(); ++j) {
            unsigned char c = raw[i + j];
            detail += (c >= 32 && c < 127) ? QChar(c) : QChar('.');
        }
        detail += QStringLiteral("\n");
    }
    detail += QStringLiteral("\n");

    // IP 快速信息 (非阻塞)
    for (const auto &ip : ips) {
        std::string ip_std = ip.toStdString();
        Nezha::IPAddress::ipaddr addr;
        Nezha::IPAddress::ipaddr::parse(ip_std, addr);

        detail += QStringLiteral("── IP: %1").arg(ip);
        detail += QStringLiteral("  内网:%1").arg(addr.is_private() ? QStringLiteral("是") : QStringLiteral("否"));
        detail += QStringLiteral("  回环:%1\n").arg(addr.is_loopback() ? QStringLiteral("是") : QStringLiteral("否"));
    }

    // 异步查询 GeoIP + 主机名
    ui->log_detail->setPlainText(detail);
    if (!ips.isEmpty()) {
        QStringList ips_copy = ips;
        auto future = QtConcurrent::run([this, ips_copy]() mutable {
            for (const auto &ip : ips_copy) {
                std::string ip_std = ip.toStdString();
                auto geo = Nezha::Core::GeoIP::lookup(ip_std);
                std::string host = Nezha::IPAddress::ipaddr::ResolveHostname(ip_std);

                QString extra;
                if (!host.empty() && host != ip_std)
                    extra += QStringLiteral("  主机名: %1\n").arg(QString::fromStdString(host));
                if (geo.valid) {
                    extra += QStringLiteral("  国家: %1 (%2)")
                        .arg(QString::fromStdString(geo.country), QString::fromStdString(geo.country_code));
                    if (!geo.city.empty())
                        extra += QStringLiteral("  城市: %1").arg(QString::fromStdString(geo.city));
                    if (geo.lat != 0.0 || geo.lon != 0.0)
                        extra += QStringLiteral("  坐标: %.4f,%.4f").arg(geo.lat).arg(geo.lon);
                    if (!geo.isp.empty())
                        extra += QStringLiteral("  ISP: %1").arg(QString::fromStdString(geo.isp));
                    extra += QStringLiteral("\n");
                }

                if (!extra.isEmpty()) {
                    QMetaObject::invokeMethod(this, [this, extra]() {
                        QString cur = ui->log_detail->toPlainText();
                        if (!cur.contains(extra))
                            ui->log_detail->setPlainText(cur + extra);
                    }, Qt::QueuedConnection);
                }
            }
        });
    }
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

void monitor::on_log_search_changed(const QString &text) {
    if (!log_search_proxy_) return;
    log_search_proxy_->setFilterFixedString(text);
}

void monitor::clear_logs() {
    if (log_model_) log_model_->clear();
    if (alert_model_) alert_model_->clear();
    if (honey_model_) honey_model_->clear();
    ui->card_logs_value->setText(QStringLiteral("0"));
    ui->card_alerts_value->setText(QStringLiteral("0"));
    ui->log_detail->clear();
    ui->alert_detail->clear();
    ui->honey_detail->clear();
}

void monitor::update_sparkline() {
    int cur = log_model_ ? log_model_->total() : 0;
    static int last = 0;
    int delta = cur - last;
    if (delta < 0) delta = 0;
    last = cur;
    sparkline_data_.append(delta);
    if (sparkline_data_.size() > 60)
        sparkline_data_.removeFirst();
    if (sparkline_widget_) static_cast<SparklineWidget *>(sparkline_widget_)->set_data(sparkline_data_);
}
