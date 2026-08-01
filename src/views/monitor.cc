#include "monitor.h"
#include "ui_monitor.h"
#include "log_model.h"
#include "gui_sink.h"
#include "detail_panel.h"
#include "theme.h"
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
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSequentialAnimationGroup>
#include <QSortFilterProxyModel>
#include <QStyleHints>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

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

// =============================================================================
// SparklineWidget
// =============================================================================
void SparklineWidget::set_data(const QList<int> &d) { data_ = d; update(); }

void SparklineWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QColor bg = dark ? QColor(Theme::DkCard) : QColor(Theme::LtCard);
    QColor fg = dark ? QColor(Theme::Pink) : QColor(Theme::PinkDeep);
    QColor gr = dark ? QColor(Theme::DkBorder) : QColor(Theme::LtBorder);

    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 6, 6);

    if (data_.isEmpty()) {
        p.setPen(fg);
        p.setFont(QFont(QStringLiteral("Inter"), 11));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("等待数据..."));
        return;
    }

    int n = data_.size();
    int maxv = *std::max_element(data_.begin(), data_.end());
    if (maxv == 0) maxv = 1;
    QRect r = rect().adjusted(12, 16, -12, -28);
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
    fill.lineTo(r.right(), r.bottom());
    fill.closeSubpath();

    QLinearGradient grad(0, r.top(), 0, r.bottom());
    grad.setColorAt(0.0, QColor(255, 255, 255, 60));
    grad.setColorAt(1.0, QColor(Theme::Pink).darker(130));
    QColor fc = QColor(Theme::Pink);
    fc.setAlpha(20);
    p.setPen(Qt::NoPen);
    p.setBrush(fc);
    p.drawPath(fill);

    for (int i = 1; i < n; ++i) {
        double t = static_cast<double>(i) / n;
        QColor segColor(
            static_cast<int>(255 * (1 - t) + QColor(Theme::Pink).red() * t),
            static_cast<int>(255 * (1 - t) + QColor(Theme::Pink).green() * t),
            static_cast<int>(255 * (1 - t) + QColor(Theme::Pink).blue() * t)
        );
        p.setPen(QPen(segColor, 1.5));
        p.drawLine(QPointF(r.left() + w * (i - 1), r.bottom() - (data_[i - 1] * h / maxv)),
                   QPointF(r.left() + w * i,     r.bottom() - (data_[i] * h / maxv)));
    }

    for (int i = 1; i < n; i += 4) {
        double t = static_cast<double>(i) / n;
        QColor dotColor(
            static_cast<int>(255 * (1 - t) + QColor(Theme::PinkDeep).red() * t),
            static_cast<int>(255 * (1 - t) + QColor(Theme::PinkDeep).green() * t),
            static_cast<int>(255 * (1 - t) + QColor(Theme::PinkDeep).blue() * t)
        );
        p.setBrush(dotColor);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(r.left() + w * i, r.bottom() - (data_[i] * h / maxv)), 2.0, 2.0);
    }

    p.setPen(fg);
    p.setFont(QFont(QStringLiteral("Inter"), 9));
    p.drawText(QRect(r.left(), r.bottom() + 4, r.width() / 2, 20),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("%1s").arg(n));
    p.drawText(QRect(r.left(), r.bottom() + 4, r.width(), 20),
               Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("max %1").arg(maxv));
}

void LogDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const {
    QStyleOptionViewItem o = opt;
    initStyleOption(&o, idx);
    p->save();
    QColor bg = dark ? (idx.row() % 2 ? QColor(Theme::DkCard) : QColor(Theme::DkBg))
                     : (idx.row() % 2 ? QColor(Theme::LtHover) : QColor(Theme::LtCard));
    QColor fg = idx.data(LogModel::ColorRole).value<QColor>();
    if (!fg.isValid()) fg = dark ? QColor(Theme::PinkLight) : QColor(Theme::PinkDeep);
    if (o.state & QStyle::State_Selected) bg = dark ? QColor(Theme::DkSelected) : QColor(Theme::LtSelected);
    p->fillRect(o.rect, bg);
    QString tm = idx.data(LogModel::TimestampRole).toString();
    QString msg = idx.data(LogModel::MessageRole).toString();
    QString lv = idx.data(LogModel::LevelRole).toString();
    QFont mf(QStringLiteral("Menlo"), 10); mf.setStyleHint(QFont::Monospace); p->setFont(mf);
    QRect r = o.rect.adjusted(10, 2, -10, -2); int x = r.x();
    p->setPen(dark ? QColor(Theme::DkMuted) : QColor(Theme::LtMuted));
    p->drawText(x, r.y(), r.width(), r.height(), Qt::AlignLeft | Qt::AlignVCenter, tm);
    x += p->fontMetrics().horizontalAdvance(tm) + 10;
    QRect bd(x, r.y() + 3, 48, r.height() - 6);
    p->setRenderHint(QPainter::Antialiasing); p->setBrush(fg.darker(dark ? 150 : 120)); p->setPen(Qt::NoPen);
    p->drawRoundedRect(bd, 6, 6); p->setPen(QColor("#ffffff"));
    QFont bf = QApplication::font(); bf.setPointSize(8); bf.setBold(true); p->setFont(bf);
    p->drawText(bd, Qt::AlignCenter, lv); x += 56;
    p->setFont(mf); p->setPen(fg);
    p->drawText(QRect(x, r.y(), r.right() - x, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                p->fontMetrics().elidedText(msg, Qt::ElideRight, r.right() - x)); p->restore();
}
QSize LogDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const { return {200, 28}; }

void AlertDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const {
    QStyleOptionViewItem o = opt;
    initStyleOption(&o, idx);
    p->save();
    QColor bg = dark ? (idx.row() % 2 ? QColor(Theme::DkCard) : QColor(Theme::DkBg))
                     : (idx.row() % 2 ? QColor(Theme::LtHover) : QColor(Theme::LtCard));
    QColor fg = idx.data(LogModel::ColorRole).value<QColor>();
    if (!fg.isValid()) fg = dark ? QColor(Theme::Pink) : QColor(Theme::PinkDeep);
    if (o.state & QStyle::State_Selected) bg = dark ? QColor(Theme::DkSelected) : QColor(Theme::LtSelected);
    p->fillRect(o.rect, bg);
    QString tm = idx.data(LogModel::TimestampRole).toString();
    QString msg = idx.data(LogModel::MessageRole).toString();
    QString lv = idx.data(LogModel::LevelRole).toString();
    QFont mf(QStringLiteral("Menlo"), 10); mf.setStyleHint(QFont::Monospace); p->setFont(mf);
    QRect r = o.rect.adjusted(10, 2, -10, -2); int x = r.x();
    p->setPen(dark ? QColor(Theme::DkMuted) : QColor(Theme::LtMuted));
    p->drawText(x, r.y(), r.width(), r.height(), Qt::AlignLeft | Qt::AlignVCenter, tm);
    x += p->fontMetrics().horizontalAdvance(tm) + 10;
    int lw = p->fontMetrics().horizontalAdvance(lv) + 14;
    QRect bd(x, r.y() + 3, lw, r.height() - 6);
    p->setRenderHint(QPainter::Antialiasing); p->setBrush(fg); p->setPen(Qt::NoPen);
    p->drawRoundedRect(bd, 6, 6); p->setPen(QColor("#ffffff"));
    QFont bf = QApplication::font(); bf.setPointSize(8); bf.setBold(true); p->setFont(bf);
    p->drawText(bd, Qt::AlignCenter, lv); x += lw + 10;
    p->setFont(mf); p->setPen(fg);
    p->drawText(QRect(x, r.y(), r.right() - x, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                p->fontMetrics().elidedText(msg, Qt::ElideRight, r.right() - x)); p->restore();
}
QSize AlertDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const { return {200, 28}; }

// -- monitor constructor --
monitor::monitor(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::monitor)
{
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

    sparkline_timer_ = new QTimer(this);
    connect(sparkline_timer_, &QTimer::timeout, this, &monitor::update_sparkline);
    sparkline_timer_->start(1000);
}

monitor::~monitor() { delete ui; }

// -- animations --
void monitor::start_animations() {
    // title glow
    auto *glow = new QGraphicsDropShadowEffect(this);
    glow->setBlurRadius(12);
    glow->setOffset(0, 0);
    glow->setColor(QColor(Theme::Pink));
    ui->app_title->setGraphicsEffect(glow);

    auto *glow_anim = new QPropertyAnimation(glow, "blurRadius", this);
    glow_anim->setDuration(2200);
    glow_anim->setStartValue(4);
    glow_anim->setEndValue(18);
    glow_anim->setEasingCurve(QEasingCurve::InOutSine);
    glow_anim->setLoopCount(-1);
    glow_anim->start();

    auto *glow_color = new QPropertyAnimation(glow, "color", this);
    glow_color->setDuration(2800);
    glow_color->setStartValue(QColor(Theme::PinkLight));
    glow_color->setKeyValueAt(0.5, QColor(Theme::PinkDeep));
    glow_color->setEndValue(QColor(Theme::PinkLight));
    glow_color->setEasingCurve(QEasingCurve::InOutSine);
    glow_color->setLoopCount(-1);
    glow_color->start();

    // status dot pulse
    auto *pulse_min = new QPropertyAnimation(ui->status_dot, "minimumSize", this);
    pulse_min->setDuration(1600);
    pulse_min->setStartValue(QSize(7, 7));
    pulse_min->setEndValue(QSize(11, 11));
    pulse_min->setEasingCurve(QEasingCurve::InOutSine);
    pulse_min->setLoopCount(-1);

    auto *pulse_max = new QPropertyAnimation(ui->status_dot, "maximumSize", this);
    pulse_max->setDuration(1600);
    pulse_max->setStartValue(QSize(7, 7));
    pulse_max->setEndValue(QSize(11, 11));
    pulse_max->setEasingCurve(QEasingCurve::InOutSine);
    pulse_max->setLoopCount(-1);

    pulse_min->start();
    pulse_max->start();

    // staggered card entrance (fade in, then release effect for stylesheet hover)
    QFrame *cards[] = {ui->card_logs, ui->card_alerts, ui->card_threats, ui->card_uptime};
    for (int i = 0; i < 4; ++i) {
        auto *fx = new QGraphicsOpacityEffect(this);
        fx->setOpacity(0.0);
        cards[i]->setGraphicsEffect(fx);
        auto *anim = new QPropertyAnimation(fx, "opacity", this);
        anim->setDuration(500);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QPropertyAnimation::finished, this, [card = cards[i]]() {
            card->setGraphicsEffect(nullptr);
        });
        QTimer::singleShot(120 + i * 80, anim, [anim]() { anim->start(); });
    }
}

// -- theme --
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
    if (log_delegate_) log_delegate_->dark = dark;
    if (alert_delegate_) alert_delegate_->dark = dark;
    if (recent_delegate_) recent_delegate_->dark = dark;
    if (honey_delegate_) honey_delegate_->dark = dark;
    if (sparkline_widget_) sparkline_widget_->dark = dark;
    if (log_detail_panel_) log_detail_panel_->set_dark(dark);
    if (alert_detail_panel_) alert_detail_panel_->set_dark(dark);
    if (honey_detail_panel_) honey_detail_panel_->set_dark(dark);
    apply_stylesheet(dark);
}

void monitor::apply_stylesheet(bool dark) {
    auto bg = dark ? Theme::DkBg : Theme::LtBg;
    auto card = dark ? Theme::DkCard : Theme::LtCard;
    auto border = dark ? Theme::DkBorder : Theme::LtBorder;
    auto text = dark ? Theme::DkText : Theme::LtText;
    auto accent = dark ? Theme::White : Theme::PinkDeep;
    auto muted = dark ? Theme::DkMuted : Theme::LtMuted;
    auto selected = dark ? Theme::DkSelected : Theme::LtSelected;
    auto hover = dark ? Theme::DkHover : Theme::LtHover;
    auto pink = dark ? Theme::Pink : Theme::PinkDeep;
    auto altBg = dark ? Theme::DkCard : Theme::LtHover;

    setStyleSheet(QStringLiteral(R"(
        * { font-family:"Inter","SF Pro Display","PingFang SC",sans-serif; }
        QMainWindow { background:%1; }
        #header { background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 %2,stop:1 %8); border-bottom:1px solid %3; }
        QLabel { color:%4; }
        #app_title { font-size:15px; font-weight:700; color:%5; letter-spacing:-0.2px; }
        #brand_badge { background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %5,stop:1 %9); color:%1; border-radius:10px; font-size:9px; font-weight:700; padding:1px 7px; }
        #clock_label { font-size:10px; color:%6; }
        #status_dot { background:%5; border-radius:4px; }
        #status_text { font-size:10px; color:%5; font-weight:600; }
        QStatusBar { background:%2; border-top:1px solid %3; font-size:10px; }
        #sidebar { background:%2; border-right:1px solid %3; }
        #sidebar::item { color:%6; padding:12px 22px; font-size:12px; border:none; margin:1px 6px; border-radius:12px; }
        #sidebar::item:selected { background:%7; color:%5; font-weight:600; border-left:2px solid %5; }
        #sidebar::item:hover:!selected { background:%8; color:%4; }
        #recent_alerts_label, #dash_title, #logs_title, #alerts_title, #honey_title, #network_title,
        #local_ip_label, #arp_label, #quarantine_label { font-size:12px; font-weight:600; color:%9; padding-bottom:4px; border-bottom:1px solid %3; }
        QFrame#card_logs, QFrame#card_alerts, QFrame#card_threats, QFrame#card_uptime {
            background:%2; border:1px solid %3; border-radius:16px; padding:16px 14px; border-left:3px solid %3; }
        QFrame#card_logs { border-left-color:%9; }
        QFrame#card_alerts { border-left-color:%5; }
        QFrame#card_threats { border-left-color:#ff8a80; }
        QFrame#card_uptime { border-left-color:#a5d6a7; }
        QFrame#card_logs:hover, QFrame#card_alerts:hover, QFrame#card_threats:hover, QFrame#card_uptime:hover { border-color:%9; background:%8; }
        #card_logs_value, #card_alerts_value, #card_threats_value, #card_uptime_value { font-size:28px; font-weight:800; color:%4; }
        #card_logs_label, #card_alerts_label, #card_threats_label, #card_uptime_label { font-size:10px; font-weight:600; color:%6; margin-top:4px; }
        QTableView { background:%1; alternate-background-color:%10; gridline-color:%3;
                     color:%4; border:1px solid %3; border-radius:12px; font-size:11px; }
        QTableView::item:selected { background:%7; }
        QHeaderView::section { background:%2; color:%6; padding:6px 12px;
                               border:none; border-bottom:1px solid %3; font-size:10px; font-weight:600; }
        QComboBox { background:%2; color:%4; border:1px solid %3; border-radius:12px;
                    padding:5px 12px; font-size:11px; min-width:80px; }
        QComboBox:hover { border-color:%5; }
        QComboBox::drop-down { border:none; width:20px; }
        QComboBox QAbstractItemView { background:%2; color:%4; selection-background-color:%7; border:1px solid %3; border-radius:6px; }
        QPushButton { background:%2; color:%5; border:1px solid %3; border-radius:12px; padding:6px 16px; font-size:11px; font-weight:600; }
        QPushButton:hover { background:%8; border-color:%9; color:%9; }
        QPushButton:pressed { background:%7; }
        QTableWidget { background:%1; gridline-color:%3; color:%4; border:1px solid %3; border-radius:12px; font-size:11px; }
        QTableWidget::item:selected { background:%7; }
        QTextEdit { background:%1; color:%4; border:1px solid %3; border-radius:12px; font-size:12px; padding:8px; }
        QLineEdit { background:%2; color:%4; border:1px solid %3; border-radius:12px; padding:6px 12px; font-size:11px; }
        QLineEdit:focus { border-color:%5; }
        QScrollBar:vertical { background:transparent; width:5px; margin:1px; }
        QScrollBar::handle:vertical { background:%3; border-radius:2px; min-height:20px; }
        QScrollBar::handle:vertical:hover { background:%5; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
    )")
        .arg(bg, card, border, text, accent, muted, selected, hover, pink, altBg));
}

// -- setup --
// =============================================================================
void monitor::setup_sidebar() {
    ui->sidebar->setCurrentRow(0);
}

void monitor::update_clock() {
    ui->clock_label->setText(
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd  hh:mm:ss")));
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

// -- models --
// =============================================================================
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
    log_delegate_->dark = dark_mode_;
    ui->log_view->setItemDelegate(log_delegate_);
    ui->log_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->log_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(ui->log_view, &QTableView::clicked, this, &monitor::show_log_detail);

    connect(ui->level_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &monitor::apply_log_filter);

    log_search_proxy_ = new QSortFilterProxyModel(this);
    log_search_proxy_->setSourceModel(log_proxy_);
    log_search_proxy_->setFilterRole(LogModel::MessageRole);
    log_search_proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    ui->log_view->setModel(log_search_proxy_);
    connect(ui->log_search_box, &QLineEdit::textChanged, this, &monitor::log_search_changed);
    connect(ui->log_clear_btn, &QPushButton::clicked, this, &monitor::clear_logs);

    alert_model_ = new LogModel(this);
    alert_proxy_ = new QSortFilterProxyModel(this);
    alert_proxy_->setSourceModel(alert_model_);
    alert_proxy_->setFilterRole(LogModel::LevelRole);
    alert_proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);

    setup_log_table(ui->alert_view);
    ui->alert_view->setModel(alert_proxy_);
    alert_delegate_ = new AlertDelegate(this);
    alert_delegate_->dark = dark_mode_;
    ui->alert_view->setItemDelegate(alert_delegate_);
    ui->alert_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->alert_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(ui->alert_view, &QTableView::clicked, this, &monitor::show_alert_detail);
    connect(ui->alert_severity_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &monitor::filter_alert_severity);

    setup_log_table(ui->recent_alerts_view);
    ui->recent_alerts_view->setModel(alert_model_);
    recent_delegate_ = new AlertDelegate(this);
    recent_delegate_->dark = dark_mode_;
    ui->recent_alerts_view->setItemDelegate(recent_delegate_);
    ui->recent_alerts_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->recent_alerts_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(ui->recent_alerts_view, &QTableView::clicked, this, &monitor::show_alert_detail);

    honey_model_ = new LogModel(this);
    setup_log_table(ui->honey_view);
    ui->honey_view->setModel(honey_model_);
    honey_delegate_ = new LogDelegate(this);
    honey_delegate_->dark = dark_mode_;
    ui->honey_view->setItemDelegate(honey_delegate_);
    ui->honey_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->honey_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(ui->honey_view, &QTableView::clicked, this, &monitor::show_honey_detail);

    setup_network_table(ui->local_ip_table);
    setup_network_table(ui->arp_table);
    setup_network_table(ui->quarantine_table);
    setup_network_table(ui->attackers_table);
    connect(ui->refresh_network, &QPushButton::clicked, this, &monitor::refresh_network_info);
    refresh_network_info();

    sparkline_widget_ = new SparklineWidget();
    sparkline_widget_->dark = dark_mode_;
    sparkline_widget_->setObjectName(QStringLiteral("sparkline_widget"));
    if (ui->sparkline_frame->layout()) {
        delete ui->sparkline_frame->layout();
    }
    auto *vbl = new QVBoxLayout(ui->sparkline_frame);
    vbl->setContentsMargins(0, 0, 0, 0);
    vbl->addWidget(sparkline_widget_);

    // replace QTextEdit detail placeholders with DetailPanel widgets
    auto replace_detail = [this](QTextEdit *old, DetailPanel *&panel, int maxH) {
        auto *parentLayout = qobject_cast<QVBoxLayout *>(old->parentWidget()->layout());
        if (!parentLayout) return;
        int idx = parentLayout->indexOf(old);
        old->hide();
        panel = new DetailPanel(this);
        panel->setMaximumHeight(maxH);
        panel->set_dark(dark_mode_);
        if (idx >= 0) parentLayout->insertWidget(idx, panel);
    };
    replace_detail(ui->log_detail, log_detail_panel_, 200);
    replace_detail(ui->alert_detail, alert_detail_panel_, 130);
    replace_detail(ui->honey_detail, honey_detail_panel_, 130);

    // context menus
    auto build_log_ctx = [this](const QPoint &pos, QTableView *view, QSortFilterProxyModel *proxy) {
        QModelIndex idx = view->indexAt(pos);
        if (!idx.isValid()) return;
        QMenu menu(this);
        QAction *cpy = menu.addAction(QStringLiteral("复制内容"));
        QAction *geo = menu.addAction(QStringLiteral("查询 GeoIP"));
        QAction *qip = menu.addAction(QStringLiteral("隔离此 IP"));
        QAction *act = menu.exec(view->viewport()->mapToGlobal(pos));
        if (!act) return;
        QString msg = proxy
            ? proxy->data(idx.siblingAtColumn(2), LogModel::MessageRole).toString()
            : idx.data(LogModel::MessageRole).toString();
        QRegularExpression ipr(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");
        QString ip = ipr.match(msg).captured(1);
        if (act == cpy) {
            QApplication::clipboard()->setText(msg);
        } else if (act == geo && !ip.isEmpty()) {
            show_log_detail(idx);
        } else if (act == qip && !ip.isEmpty()) {
            Nezha::Database::DatabaseHelper::QuarantineIP(
                ip.toStdString(), QStringLiteral("手动隔离").toStdString(), 50.0);
            refresh_quarantine_list();
        }
    };

    ui->log_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->log_view, &QTableView::customContextMenuRequested, this,
            [this, build_log_ctx](const QPoint &p) {
                build_log_ctx(p, ui->log_view, log_search_proxy_); });

    ui->alert_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->alert_view, &QTableView::customContextMenuRequested, this,
            [this, build_log_ctx](const QPoint &p) {
                build_log_ctx(p, ui->alert_view, alert_proxy_); });

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
                Nezha::Database::DatabaseHelper::QuarantineIP(
                    ip.toStdString(), QStringLiteral("蜜罐手动隔离").toStdString(), 75.0);
                refresh_quarantine_list();
            }
        }
    });

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

// -- stats --
// =============================================================================
void monitor::update_stats(int log_count, int alert_count) {
    static int last_log = 0, last_alert = 0;
    int secs = std::max(1, start_time_.secsTo(QTime::currentTime()));
    int logRate = (log_count - last_log);
    int alertRate = (alert_count - last_alert);
    last_log = log_count; last_alert = alert_count;
    if (logRate < 0) logRate = 0;
    if (alertRate < 0) alertRate = 0;

    int qcount = static_cast<int>(Nezha::Database::DatabaseHelper::GetQuarantineList().size());
    ui->status_label->setText(
        QStringLiteral("运行中 | 日志 %1 | 告警 %2 | 已隔离 %3")
            .arg(log_count).arg(alert_count).arg(qcount));

    auto set_card = [](QLabel *val, QLabel *sub, int n, int rate, const QString &unit) {
        val->setText(QString::number(n));
        sub->setText(QStringLiteral("%1  |  +%2 %3/s").arg(unit).arg(rate));
    };
    set_card(ui->card_logs_value, ui->card_logs_label, log_count, logRate, QStringLiteral("日志"));
    set_card(ui->card_alerts_value, ui->card_alerts_label, alert_count, alertRate, QStringLiteral("告警"));
    set_card(ui->card_threats_value, ui->card_threats_label, qcount, 0, QStringLiteral("已隔离"));

    int h = secs / 3600, m = (secs % 3600) / 60, s = secs % 60;
    ui->card_uptime_value->setText(QStringLiteral("%1h %2m").arg(h).arg(m));
    ui->card_uptime_label->setText(QStringLiteral("运行时间  |  %3s").arg(s));
}

void monitor::append_alert(const QString &time, const QString &type, const QString &ip,
                           int count, double score, const QString &severity) {
    if (!alert_model_) return;
    QString sc = severity.startsWith(QStringLiteral("CRIT")) ? QStringLiteral("严重")
               : severity.startsWith(QStringLiteral("ERR"))  ? QStringLiteral("错误")
               : severity.startsWith(QStringLiteral("WARN")) ? QStringLiteral("警告")
               :                                               QStringLiteral("信息");
    QString msg = QStringLiteral("%1  |  %2  |  x%3  |  %4分")
                      .arg(type, ip).arg(count).arg(static_cast<int>(score));
    alert_model_->append(time, severity, msg);
    int a = alert_model_->total();
    ui->card_alerts_value->setText(QString::number(a));
    ui->alert_stats_label->setText(QStringLiteral("共 %1 条告警").arg(a));

    if (severity.startsWith(QStringLiteral("CRIT"))) sev_crit_++;
    else if (severity.startsWith(QStringLiteral("ERR"))) sev_error_++;
    else if (severity.startsWith(QStringLiteral("WARN"))) sev_warn_++;
    else sev_info_++;

    auto sev_style = [](QLabel *l, const QString &color, int n) {
        l->setText(QStringLiteral("%1\n%2").arg(l->text().split('\n')[0]).arg(n));
        l->setStyleSheet(QStringLiteral(
            "font-family:\"Menlo\";font-size:10px;font-weight:700;color:%1;"
            "background:%2;border-radius:10px;padding:4px 0;").arg(color, Theme::DkCard));
    };
    sev_style(ui->sev_crit, Theme::Red, sev_crit_);
    sev_style(ui->sev_error, Theme::Orange, sev_error_);
    sev_style(ui->sev_warn, Theme::Pink, sev_warn_);
    sev_style(ui->sev_info, Theme::DkMuted, sev_info_);
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

void monitor::record_attacker(const QString &ip, double score, const QString &type) {
    auto &a = attackers_[ip];
    a.score = std::max(a.score, score);
    a.count++;
    if (!type.isEmpty()) a.type = type;
    refresh_attackers();
}

void monitor::refresh_attackers() {
    auto *t = ui->attackers_table;
    t->setRowCount(0);
    t->setHorizontalHeaderLabels({QStringLiteral("IP"), QStringLiteral("国家"), QStringLiteral("评分"), QStringLiteral("次数")});

    QList<std::pair<QString, Attacker>> sorted;
    for (auto it = attackers_.begin(); it != attackers_.end(); ++it)
        sorted.append({it.key(), it.value()});
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
        return a.second.score * a.second.count > b.second.score * b.second.count;
    });
    if (sorted.size() > 10) sorted = sorted.mid(0, 10);

    QFont mf(QStringLiteral("Menlo"), 10);
    mf.setStyleHint(QFont::Monospace);
    for (const auto &[ip, a] : sorted) {
        int r = t->rowCount(); t->insertRow(r);
        auto *ipItem = new QTableWidgetItem(ip); ipItem->setFont(mf);
        ipItem->setForeground(QColor(Theme::Pink));
        auto *countryItem = new QTableWidgetItem(QStringLiteral("..."));
        countryItem->setForeground(QColor(Theme::DkMuted));
        auto *scoreItem = new QTableWidgetItem(QString::number(a.score, 'f', 0));
        scoreItem->setForeground(QColor(Theme::PinkDeep));
        auto *countItem = new QTableWidgetItem(QString::number(a.count));
        countItem->setForeground(QColor(Theme::DkText));
        t->setItem(r, 0, ipItem);
        t->setItem(r, 1, countryItem);
        t->setItem(r, 2, scoreItem);
        t->setItem(r, 3, countItem);

        // async geo lookup
        QString ipCopy = ip;
        auto _geoFuture = QtConcurrent::run([this, ipCopy, r]() {
            auto geo = Nezha::Core::GeoIP::lookup(ipCopy.toStdString());
            if (geo.valid) {
                QMetaObject::invokeMethod(this, [this, r, geo]() {
                    auto *item = ui->attackers_table->item(r, 1);
                    if (item) item->setText(QString::fromStdString(geo.country_code));
                }, Qt::QueuedConnection);
            }
        });
    }
    t->resizeColumnToContents(0);
    t->horizontalHeader()->setStretchLastSection(true);
}

// -- filters --
// =============================================================================
void monitor::apply_log_filter(int index) {
    if (!log_proxy_) return;
    log_proxy_->setFilterFixedString(index == 0 ? QString() : ui->level_filter->currentText());
}

void monitor::filter_alert_severity(int index) {
    if (!alert_proxy_) return;
    alert_proxy_->setFilterFixedString(index == 0 ? QString() : ui->alert_severity_filter->currentText());
}

void monitor::log_search_changed(const QString &text) {
    if (!log_search_proxy_) return;
    log_search_proxy_->setFilterFixedString(text);
}

// -- detail panels --
void monitor::show_log_detail(const QModelIndex &idx) {
    if (!idx.isValid() || !log_detail_panel_) return;
    QString tm = idx.data(LogModel::TimestampRole).toString();
    QString lv = idx.data(LogModel::LevelRole).toString();
    QString msg = idx.data(LogModel::MessageRole).toString();

    QRegularExpression ip_re(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");
    QStringList ips;
    auto it = ip_re.globalMatch(msg);
    while (it.hasNext()) {
        QString ip = it.next().captured(1);
        if (!ips.contains(ip)) ips.append(ip);
    }

    log_detail_panel_->show_log(tm, lv, msg, ips);

    if (!ips.isEmpty()) {
        QStringList ips_copy = ips;
        auto future = QtConcurrent::run([this, ips_copy]() mutable {
            for (const auto &ip : ips_copy) {
                std::string ip_std = ip.toStdString();
                auto geo = Nezha::Core::GeoIP::lookup(ip_std);
                std::string host = Nezha::IPAddress::ipaddr::ResolveHostname(ip_std);
                QMetaObject::invokeMethod(this, [this, ip, host, geo]() {
                    if (log_detail_panel_) {
                        log_detail_panel_->add_geo_info(
                            ip, QString::fromStdString(host), geo);
                    }
                }, Qt::QueuedConnection);
            }
        });
    }
}

void monitor::show_alert_detail(const QModelIndex &idx) {
    if (!idx.isValid() || !alert_detail_panel_) return;
    QString tm = idx.data(LogModel::TimestampRole).toString();
    QString lv = idx.data(LogModel::LevelRole).toString();
    QString msg = idx.data(LogModel::MessageRole).toString();
    alert_detail_panel_->show_alert(tm, lv, msg);
}

void monitor::show_honey_detail(const QModelIndex &idx) {
    if (!idx.isValid() || !honey_detail_panel_) return;
    QString tm = idx.data(LogModel::TimestampRole).toString();
    QString msg = idx.data(LogModel::MessageRole).toString();
    honey_detail_panel_->show_honeypot(tm, msg);
}

// -- network --
// =============================================================================
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
        int r = t->rowCount();
        t->insertRow(r);
        auto *n = new QTableWidgetItem(ifa->ifa_name);
        n->setForeground(QColor(Theme::PinkLight));
        auto *i = new QTableWidgetItem(ip);
        i->setFont(mf);
        i->setForeground(QColor("#3fb950"));
        t->setItem(r, 0, n);
        t->setItem(r, 1, i);
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
        if (!(rtm->rtm_flags & RTF_LLINFO) ||
            (rtm->rtm_flags & (RTF_LOCAL | RTF_BROADCAST | RTF_MULTICAST))) {
            p += rtm->rtm_msglen;
            continue;
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
        int row = t->rowCount();
        t->insertRow(row);
        auto *ipx = new QTableWidgetItem(ips);
        ipx->setFont(mf);
        ipx->setForeground(QColor("#3fb950"));
        auto *mx = new QTableWidgetItem(mb);
        mx->setFont(mf);
        mx->setForeground(QColor(Theme::DkMuted));
        t->setItem(row, 0, ipx);
        t->setItem(row, 1, mx);
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
        int row = t->rowCount();
        t->insertRow(row);
        auto *ip = new QTableWidgetItem(QString::fromStdString(r.ip_address));
        ip->setFont(mf);
        ip->setForeground(QColor("#f85149"));
        auto *reason = new QTableWidgetItem(QString::fromStdString(r.reason));
        reason->setForeground(QColor(Theme::Pink));
        auto *score = new QTableWidgetItem(QString::number(r.threat_score, 'f', 0));
        score->setForeground(QColor(Theme::DkMuted));
        t->setItem(row, 0, ip);
        t->setItem(row, 1, reason);
        t->setItem(row, 2, score);
    }
    t->resizeColumnToContents(0);
    t->horizontalHeader()->setStretchLastSection(true);
}

// -- misc --
// =============================================================================
void monitor::clear_logs() {
    sev_crit_ = sev_error_ = sev_warn_ = sev_info_ = 0;
    if (log_model_) log_model_->clear();
    if (alert_model_) alert_model_->clear();
    if (honey_model_) honey_model_->clear();
    ui->card_logs_value->setText(QStringLiteral("0"));
    ui->card_alerts_value->setText(QStringLiteral("0"));
    if (log_detail_panel_) log_detail_panel_->clear();
    if (alert_detail_panel_) alert_detail_panel_->clear();
    if (honey_detail_panel_) honey_detail_panel_->clear();
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
    if (sparkline_widget_)
        sparkline_widget_->set_data(sparkline_data_);
}
