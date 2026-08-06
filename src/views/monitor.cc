#include "monitor.h"
#include "ui_monitor.h"
#include "log_model.h"
#include "gui_sink.h"
#include "detail_panel.h"
#include "radar_widget.h"
#include "theme.h"
#include <unistd.h>

#include "../contants.h"
#include "../service/database_helper.h"
#include "../core/geo_ip.h"
#include "../core/ipaddr.h"
#include "../core/protocol_stats.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QSettings>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QSystemTrayIcon>
#include <QTextBrowser>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QPushButton>
#include <QRegularExpression>
#include <QScroller>
#include <QShortcut>
#include <QGestureEvent>
#include <QPinchGesture>
#include <QWheelEvent>
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
#include <netinet/in.h>
#include <sys/socket.h>

#if defined(__APPLE__)
#include <net/if_dl.h>
#include <net/route.h>
#include <sys/sysctl.h>
#endif

#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

// SparklineWidget
void SparklineWidget::set_data(const QList<int> &d) {
    data_ = d;
    update();
}

void SparklineWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    auto bg = QColor(dark ? Theme::DkCard : Theme::LtCard);
    auto fg = QColor(dark ? Theme::Seafoam : Theme::SeafoamDeep);
    auto gr = QColor(dark ? Theme::DkBorder : Theme::LtBorder);

    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 14, 14);

    if (data_.isEmpty()) {
        p.setPen(fg);
        p.setFont(QFont(QStringLiteral("SF Pro Rounded"), 11));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("--"));
        return;
    }

    int n = data_.size();
    int maxv = std::max(1, *std::max_element(data_.begin(), data_.end()));
    QRect r = rect().adjusted(12, 16, -12, -28);
    double w = static_cast<double>(r.width()) / (n - 1);
    double h = static_cast<double>(r.height());

    // subtle grid lines
    p.setPen(QPen(gr, 0.5, Qt::DotLine));
    for (int i = 1; i <= 3; ++i)
        p.drawLine(QPointF(r.left(), r.top() + h * i / 4),
                   QPointF(r.right(), r.top() + h * i / 4));

    // gradient fill under the curve
    QPainterPath fill;
    fill.moveTo(r.left(), r.bottom());
    for (int i = 0; i < n; ++i)
        fill.lineTo(QPointF(r.left() + w * i, r.bottom() - (data_[i] * h / maxv)));
    fill.lineTo(r.right(), r.bottom());
    fill.closeSubpath();

    QLinearGradient grad(r.topLeft(), r.bottomLeft());
    QColor gs = fg; gs.setAlpha(35);
    QColor ge = fg; ge.setAlpha(0);
    grad.setColorAt(0.0, gs);
    grad.setColorAt(1.0, ge);
    p.setBrush(grad);
    p.setPen(Qt::NoPen);
    p.drawPath(fill);

    // line
    p.setPen(QPen(fg, 2.0));
    for (int i = 1; i < n; ++i)
        p.drawLine(QPointF(r.left() + w * (i - 1), r.bottom() - (data_[i - 1] * h / maxv)),
                   QPointF(r.left() + w * i, r.bottom() - (data_[i] * h / maxv)));

    // label
    p.setPen(fg);
    p.setFont(QFont(QStringLiteral("SF Mono"), 9));
    p.drawText(QRect(r.left(), r.bottom() + 4, r.width(), 22),
               Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("%1 点  峰值 %2").arg(n).arg(maxv));
}

// Delegates
void LogDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const {
    QStyleOptionViewItem o = opt;
    initStyleOption(&o, idx);
    p->save();

    auto bg = dark
        ? (idx.row() % 2 ? QColor(Theme::DkCard) : QColor(Theme::DkBg))
        : (idx.row() % 2 ? QColor(Theme::LtCardHover) : QColor(Theme::LtCard));
    if (o.state & QStyle::State_Selected)
        bg = dark ? QColor(Theme::DkSelected) : QColor(Theme::LtSelected);
    p->fillRect(o.rect, bg);

    QColor fg = idx.data(LogModel::ColorRole).value<QColor>();
    if (!fg.isValid()) fg = dark ? QColor(Theme::SeafoamLight) : QColor(Theme::SeafoamDeep);

    QString tm = idx.data(LogModel::TimestampRole).toString();
    QString msg = idx.data(LogModel::MessageRole).toString();
    QString lv = idx.data(LogModel::LevelRole).toString();

    QFont mf(QStringLiteral("SF Mono"), 10);
    mf.setStyleHint(QFont::Monospace);
    p->setFont(mf);
    QRect r = o.rect.adjusted(12, 2, -12, -2);
    int x = r.x();

    // timestamp
    p->setPen(dark ? QColor(Theme::DkMuted) : QColor(Theme::LtMuted));
    p->drawText(x, r.y(), r.width(), r.height(), Qt::AlignLeft | Qt::AlignVCenter, tm);
    x += p->fontMetrics().horizontalAdvance(tm) + 12;

    // level badge — softer pill shape
    auto badgeW = p->fontMetrics().horizontalAdvance(lv) + 16;
    QRect bd(x, r.y() + 2, badgeW, r.height() - 4);
    p->setRenderHint(QPainter::Antialiasing);
    {
        QColor badgeBg = fg;
        badgeBg.setAlpha(dark ? 200 : 180);
        p->setBrush(badgeBg);
    }
    p->setPen(Qt::NoPen);
    p->drawRoundedRect(bd, 7, 7);
    p->setPen(dark ? QColor(Theme::DkBg) : QColor(Theme::LtCard));
    QFont bf(QStringLiteral("SF Pro Rounded"), 9);
    bf.setBold(true);
    bf.setStyleHint(QFont::SansSerif);
    p->setFont(bf);
    p->drawText(bd, Qt::AlignCenter, lv);
    x += badgeW + 12;

    // message
    p->setFont(mf);
    p->setPen(fg);
    p->drawText(QRect(x, r.y(), r.right() - x, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                p->fontMetrics().elidedText(msg, Qt::ElideRight, r.right() - x));
    p->restore();
}

QSize LogDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const { return {200, 30}; }

void AlertDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const {
    QStyleOptionViewItem o = opt;
    initStyleOption(&o, idx);
    p->save();

    auto bg = dark
        ? (idx.row() % 2 ? QColor(Theme::DkCard) : QColor(Theme::DkBg))
        : (idx.row() % 2 ? QColor(Theme::LtCardHover) : QColor(Theme::LtCard));
    if (o.state & QStyle::State_Selected)
        bg = dark ? QColor(Theme::DkSelected) : QColor(Theme::LtSelected);
    p->fillRect(o.rect, bg);

    QColor fg = idx.data(LogModel::ColorRole).value<QColor>();
    if (!fg.isValid()) fg = dark ? QColor(Theme::Sakura) : QColor(Theme::SakuraDeep);

    QString tm = idx.data(LogModel::TimestampRole).toString();
    QString msg = idx.data(LogModel::MessageRole).toString();
    QString lv = idx.data(LogModel::LevelRole).toString();

    QFont mf(QStringLiteral("SF Mono"), 10);
    mf.setStyleHint(QFont::Monospace);
    p->setFont(mf);
    QRect r = o.rect.adjusted(12, 2, -12, -2);
    int x = r.x();

    // timestamp
    p->setPen(dark ? QColor(Theme::DkMuted) : QColor(Theme::LtMuted));
    p->drawText(x, r.y(), r.width(), r.height(), Qt::AlignLeft | Qt::AlignVCenter, tm);
    x += p->fontMetrics().horizontalAdvance(tm) + 12;

    // severity badge — softer pill
    auto badgeW = p->fontMetrics().horizontalAdvance(lv) + 16;
    QRect bd(x, r.y() + 2, badgeW, r.height() - 4);
    p->setRenderHint(QPainter::Antialiasing);
    {
        QColor badgeBg = fg;
        badgeBg.setAlpha(dark ? 200 : 180);
        p->setBrush(badgeBg);
    }
    p->setPen(Qt::NoPen);
    p->drawRoundedRect(bd, 7, 7);
    p->setPen(dark ? QColor(Theme::DkBg) : QColor(Theme::LtCard));
    QFont bf(QStringLiteral("SF Pro Rounded"), 9);
    bf.setBold(true);
    bf.setStyleHint(QFont::SansSerif);
    p->setFont(bf);
    p->drawText(bd, Qt::AlignCenter, lv);
    x += badgeW + 12;

    // message
    p->setFont(mf);
    p->setPen(fg);
    p->drawText(QRect(x, r.y(), r.right() - x, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                p->fontMetrics().elidedText(msg, Qt::ElideRight, r.right() - x));
    p->restore();
}

QSize AlertDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const { return {200, 30}; }

// monitor
monitor::monitor(QWidget *parent) : QMainWindow(parent), ui(new Ui::monitor) {
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, true);
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    start_time_ = QTime::currentTime();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    dark_mode_ = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
#else
    dark_mode_ = (qApp->palette().color(QPalette::Window).lightness() < 128);
#endif

    {
        QSettings s(QStringLiteral("NezhaGuard"), QStringLiteral("NezhaGuard"));
        const QString pref = s.value(QStringLiteral("app/theme"), QStringLiteral("dark")).toString();
        if (pref == QStringLiteral("light")) {
            dark_mode_ = false;
            ui->theme_combo->setCurrentIndex(1);
        } else if (pref == QStringLiteral("dark")) {
            dark_mode_ = true;
            ui->theme_combo->setCurrentIndex(2);
        } else {
            ui->theme_combo->setCurrentIndex(0);
        }
    }

    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, &monitor::sync_theme);
    connect(ui->theme_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &monitor::update_theme_preference);

    // brand badge — use app icon image, not cramped text
    ui->brand_badge->setMinimumSize(36, 36);
    ui->brand_badge->setMaximumSize(36, 36);
    ui->brand_badge->setText(QString());
    ui->brand_badge->setPixmap(QPixmap(QStringLiteral(":/app_icon.svg")).scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->brand_badge->setScaledContents(true);

    // dashboard cards — give enough height for 32px value + label
    for (auto *card : {ui->card_logs, ui->card_alerts, ui->card_threats, ui->card_uptime}) {
        card->setMinimumHeight(120);
        card->setMaximumHeight(120);
    }

    clock_timer_ = new QTimer(this);
    connect(clock_timer_, &QTimer::timeout, this, &monitor::update_clock);
    clock_timer_->start(1000);
    update_clock();

    setup_sidebar();
    setup_tray();
    setup_file_menu();
    start_animations();

    // 应用监控
    connect(ui->monitor_conf_save, &QPushButton::clicked, this, &monitor::save_monitor_conf);
    connect(ui->monitor_conf_load, &QPushButton::clicked, this, &monitor::load_settings_configs);
    // 数据库
    connect(ui->database_conf_save, &QPushButton::clicked, this, &monitor::save_database_conf);
    connect(ui->database_conf_load, &QPushButton::clicked, this, &monitor::load_settings_configs);
    // Slack
    connect(ui->slack_conf_save, &QPushButton::clicked, this, [this]{ save_notifier_conf(); });
    connect(ui->slack_conf_load, &QPushButton::clicked, this, &monitor::load_settings_configs);
    // Discord
    connect(ui->discord_conf_save, &QPushButton::clicked, this, [this]{ save_notifier_conf(); });
    connect(ui->discord_conf_load, &QPushButton::clicked, this, &monitor::load_settings_configs);
    // 钉钉
    connect(ui->dingtalk_conf_save, &QPushButton::clicked, this, [this]{ save_notifier_conf(); });
    connect(ui->dingtalk_conf_load, &QPushButton::clicked, this, &monitor::load_settings_configs);
    // 飞书
    connect(ui->feishu_conf_save, &QPushButton::clicked, this, [this]{ save_notifier_conf(); });
    connect(ui->feishu_conf_load, &QPushButton::clicked, this, &monitor::load_settings_configs);
    // 企业微信
    connect(ui->wechat_conf_save, &QPushButton::clicked, this, [this]{ save_notifier_conf(); });
    connect(ui->wechat_conf_load, &QPushButton::clicked, this, &monitor::load_settings_configs);
    // 邮件
    connect(ui->email_conf_save, &QPushButton::clicked, this, [this]{ save_notifier_conf(); });
    connect(ui->email_conf_load, &QPushButton::clicked, this, &monitor::load_settings_configs);
    // Telegram
    connect(ui->telegram_conf_save, &QPushButton::clicked, this, [this]{ save_notifier_conf(); });
    connect(ui->telegram_conf_load, &QPushButton::clicked, this, &monitor::load_settings_configs);
    // 本地通知
    connect(ui->local_conf_save, &QPushButton::clicked, this, [this]{ save_notifier_conf(); });
    connect(ui->local_conf_load, &QPushButton::clicked, this, &monitor::load_settings_configs);

    // shortcuts
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+F")), this, [this]() {
        ui->sidebar->setCurrentRow(1);
        ui->log_search_box->setFocus();
    });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), this, [this]() { clear_logs(); });
    for (int k = 1; k <= 6; ++k) {
        new QShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(k)), this, [this, k]() {
            ui->sidebar->setCurrentRow(k - 1);
        });
    }

    // macOS standard shortcuts
    new QShortcut(QKeySequence::Quit, this, [this]() { tray_quit(); });
    new QShortcut(QKeySequence::Close, this, [this]() { hide(); });
    new QShortcut(QKeySequence::Preferences, this, [this]() {
        ui->sidebar->setCurrentRow(5);
    });

    sparkline_timer_ = new QTimer(this);
    connect(sparkline_timer_, &QTimer::timeout, this, &monitor::update_sparkline);
    sparkline_timer_->start(1000);  // 每秒更新，更流畅

    // 协议统计 + 状态栏实时刷新
    proto_timer_ = new QTimer(this);
    connect(proto_timer_, &QTimer::timeout, this, [this]() {
        refresh_protocol_stats();
        auto s = Nezha::Core::ProtocolStats::instance().snapshot();
        ui->status_label->setText(
            QStringLiteral("TCP:%1  UDP:%2  ICMP:%3  HTTP:%4  |  总包:%5  总流量:%6 MB")
                .arg(s.tcp_pkts).arg(s.udp_pkts).arg(s.icmp_pkts).arg(s.http_pkts)
                .arg(s.total_packets).arg(s.total_bytes / 1'000'000.0, 0, 'f', 1));
    });
    proto_timer_->start(2000);

    // 常规设置页 — 添加应用信息卡片
    {
        auto *infoGroup = new QFrame(ui->tab_general);
        infoGroup->setObjectName(QStringLiteral("app_info_card"));
        infoGroup->setFrameShape(QFrame::NoFrame);
        auto *infoLayout = new QVBoxLayout(infoGroup);
        infoLayout->setSpacing(6);
        infoLayout->setContentsMargins(16, 14, 16, 14);

        auto addRow = [&](const QString &key, const QString &val) {
            auto *row = new QHBoxLayout();
            auto *kl = new QLabel(key);
            kl->setMinimumWidth(80);
            kl->setStyleSheet(QStringLiteral("font-weight:600; font-size:11px;"));
            auto *vl = new QLabel(val);
            vl->setStyleSheet(QStringLiteral("font-size:11px;"));
            vl->setTextInteractionFlags(Qt::TextSelectableByMouse);
            vl->setFont(QFont(QStringLiteral("Menlo"), 10));
            row->addWidget(kl);
            row->addWidget(vl);
            row->addStretch();
            infoLayout->addLayout(row);
        };

        addRow(QStringLiteral("版本"),
               QString::fromLatin1("%1  (%2 %3)")
                   .arg(QString::fromLatin1(Nezha::Configuration::ApplicationConstants::ApplicationVersion),
                        QString::fromLatin1(__DATE__), QString::fromLatin1(__TIME__)));
        addRow(QStringLiteral("日志路径"), QStringLiteral("logs/nezha.log"));
        addRow(QStringLiteral("隔离数据库"), QStringLiteral("data/nezha_quarantine.db"));
        addRow(QStringLiteral("规则文件"), QStringLiteral("rules/default.yaml"));
        addRow(QStringLiteral("配置目录"), QStringLiteral("config/ (notifier + database + monitor_apps)"));
        addRow(QStringLiteral("进程 PID"), QString::number(getpid()));
        addRow(QStringLiteral("Qt 版本"), QString::fromLatin1(qVersion()));

        auto *genLayout = qobject_cast<QVBoxLayout *>(ui->tab_general->layout());
        if (genLayout) {
            auto *titleLabel = new QLabel(QStringLiteral("应用信息"));
            titleLabel->setStyleSheet(QStringLiteral("font-size:12px; font-weight:700; margin-top:8px;"));
            genLayout->addWidget(titleLabel);
            genLayout->addWidget(infoGroup);
            genLayout->addStretch();
        }
    }

    setAttribute(Qt::WA_AcceptTouchEvents);
    grabGesture(Qt::PinchGesture);

    apply_theme(dark_mode_);
}

monitor::~monitor() { delete ui; }

bool monitor::event(QEvent *e) {
    // ── pinch-to-zoom (trackpad) ──
    if (e->type() == QEvent::Gesture) {
        auto *ge = dynamic_cast<QGestureEvent *>(e);
        if (!ge) return QMainWindow::event(e);

        auto *pinch = qobject_cast<QPinchGesture *>(ge->gesture(Qt::PinchGesture));
        if (pinch) {
            if (pinch->state() == Qt::GestureUpdated) {
                font_scale_ = qBound(0.6, font_scale_ * pinch->scaleFactor(), 2.5);
                int base = qBound(8, qRound(11.0 * font_scale_), 28);
                for (auto *v : {ui->log_view, ui->alert_view, ui->honey_view,
                                ui->recent_alerts_view, ui->attackers_view}) {
                    if (!v) continue;
                    auto f = v->font();
                    f.setPixelSize(base);
                    v->setFont(f);
                }
                QFont mf(QStringLiteral("SF Mono"), base);
                mf.setStyleHint(QFont::Monospace);
                for (auto *te : {ui->monitor_conf_edit, ui->database_conf_edit,
                                 ui->slack_conf_edit, ui->discord_conf_edit,
                                 ui->dingtalk_conf_edit, ui->feishu_conf_edit,
                                 ui->wechat_conf_edit, ui->email_conf_edit,
                                 ui->telegram_conf_edit, ui->local_conf_edit}) {
                    if (te) te->setFont(mf);
                }
                return true;
            }
            if (pinch->state() == Qt::GestureFinished)
                pinch->setScaleFactor(1.0);
        }
    }

    // ── trackpad horizontal scroll ↔ page switching ──
    if (e->type() == QEvent::Wheel) {
        auto *we = dynamic_cast<QWheelEvent *>(e);
        if (we && qAbs(we->angleDelta().x()) > qAbs(we->angleDelta().y()) * 2) {
            static qreal swipeAccum = 0;
            swipeAccum += we->angleDelta().x();
            int threshold = 250;
            if (swipeAccum > threshold && ui->sidebar) {
                int cur = ui->sidebar->currentRow();
                ui->sidebar->setCurrentRow(qMax(cur - 1, 0));
                swipeAccum = 0;
                return true;
            } else if (swipeAccum < -threshold && ui->sidebar) {
                int cur = ui->sidebar->currentRow();
                ui->sidebar->setCurrentRow(qMin(cur + 1, ui->sidebar->count() - 1));
                swipeAccum = 0;
                return true;
            }
        }
    }

    return QMainWindow::event(e);
}

// -- animations --
void monitor::start_animations() {
    // ── 标题柔光呼吸 ── sakura <-> wisteria <-> seafoam cycle
    auto *glow = new QGraphicsDropShadowEffect(this);
    glow->setBlurRadius(6);
    glow->setOffset(0, 0);
    glow->setColor(QColor(Theme::Sakura));
    ui->app_title->setGraphicsEffect(glow);

    auto *ga = new QPropertyAnimation(glow, "blurRadius", this);
    ga->setDuration(3200);
    ga->setStartValue(4);
    ga->setKeyValueAt(0.5, 14);
    ga->setEndValue(4);
    ga->setEasingCurve(QEasingCurve::InOutSine);
    ga->setLoopCount(-1);
    ga->start();

    auto *gc = new QPropertyAnimation(glow, "color", this);
    gc->setDuration(4800);
    gc->setStartValue(QColor(Theme::Sakura));
    gc->setKeyValueAt(0.33, QColor(Theme::Wisteria));
    gc->setKeyValueAt(0.66, QColor(Theme::Seafoam));
    gc->setEndValue(QColor(Theme::Sakura));
    gc->setEasingCurve(QEasingCurve::InOutSine);
    gc->setLoopCount(-1);
    gc->start();

    // ── 状态指示器脉冲 ──
    for (auto *s: {ui->status_dot}) {
        auto *a = new QPropertyAnimation(s, "minimumSize", this);
        a->setDuration(1800);
        a->setStartValue(QSize(8, 8));
        a->setKeyValueAt(0.5, QSize(12, 12));
        a->setEndValue(QSize(8, 8));
        a->setEasingCurve(QEasingCurve::InOutSine);
        a->setLoopCount(-1);
        a->start();
        auto *b = new QPropertyAnimation(s, "maximumSize", this);
        b->setDuration(1800);
        b->setStartValue(QSize(8, 8));
        b->setKeyValueAt(0.5, QSize(12, 12));
        b->setEndValue(QSize(8, 8));
        b->setEasingCurve(QEasingCurve::InOutSine);
        b->setLoopCount(-1);
        b->start();
    }

    // ── 仪表盘卡片入场 + 永久柔和阴影 ──
    QFrame *cards[] = {ui->card_logs, ui->card_alerts, ui->card_threats, ui->card_uptime};
    for (int i = 0; i < 4; ++i) {
        auto *fx = new QGraphicsOpacityEffect(this);
        fx->setOpacity(0.0);
        cards[i]->setGraphicsEffect(fx);
        auto *anim = new QPropertyAnimation(fx, "opacity", this);
        anim->setDuration(600);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QPropertyAnimation::finished, this, [=, this]() {
            cards[i]->setGraphicsEffect(nullptr);
            // add permanent soft shadow after fade-in
            auto *shadow = new QGraphicsDropShadowEffect(this);
            shadow->setBlurRadius(18);
            shadow->setOffset(0, 4);
            shadow->setColor(QColor(0, 0, 0, 40));
            shadow->setEnabled(true);
            cards[i]->setGraphicsEffect(shadow);
        });
        QTimer::singleShot(120 + i * 100, anim, [anim]() { anim->start(); });
    }
}


void monitor::sync_theme() {
    QSettings s(QStringLiteral("NezhaGuard"), QStringLiteral("NezhaGuard"));
    const QString pref = s.value(QStringLiteral("app/theme"), QStringLiteral("dark")).toString();

    bool dk;
    if (pref == QStringLiteral("light")) {
        dk = false;
    } else if (pref == QStringLiteral("dark")) {
        dk = true;
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        dk = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
#else
        dk = (qApp->palette().color(QPalette::Window).lightness() < 128);
#endif
    }

    if (dk != dark_mode_) {
        dark_mode_ = dk;
        apply_theme(dk);
    }
}

void monitor::update_theme_preference(int index) {
    QSettings s(QStringLiteral("NezhaGuard"), QStringLiteral("NezhaGuard"));
    bool dk;
    switch (index) {
    case 1: // 浅色
        s.setValue(QStringLiteral("app/theme"), QStringLiteral("light"));
        dk = false;
        break;
    case 2: // 深色
        s.setValue(QStringLiteral("app/theme"), QStringLiteral("dark"));
        dk = true;
        break;
    default: // 跟随系统
        s.setValue(QStringLiteral("app/theme"), QStringLiteral("auto"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        dk = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
#else
        dk = (qApp->palette().color(QPalette::Window).lightness() < 128);
#endif
        break;
    }
    if (dk != dark_mode_) {
        dark_mode_ = dk;
        apply_theme(dk);
    }
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

    auto Bg   = d ? Theme::DkBg        : Theme::LtBg;
    auto Sh   = d ? Theme::DkSheet     : Theme::LtSheet;
    auto Card = d ? Theme::DkCard      : Theme::LtCard;
    auto Br   = d ? Theme::DkBorder    : Theme::LtBorder;
    auto BrH  = d ? Theme::DkBorderHover : Theme::LtBorderHover;
    auto Tx   = d ? Theme::DkText      : Theme::LtText;
    auto Mu   = d ? Theme::DkMuted     : Theme::LtMuted;
    auto Sel  = d ? Theme::DkSelected  : Theme::LtSelected;
    auto Hov  = d ? Theme::DkCardHover : Theme::LtCardHover;
    // 四种粉各司其职: Strawberry(标题) · Peachy(卡片) · Rosy(交互) · Coral(告警)
    auto Pk   = d ? Theme::Strawberry     : Theme::RosyDeep;     // 标题/主强调
    auto PkL  = d ? Theme::StrawberryLight: Theme::RosyLight;
    auto Ros  = d ? Theme::Rosy           : Theme::RosyDeep;     // hover/交互粉
    auto Wis  = d ? Theme::Lilac          : Theme::LilacDeep;    // 薰衣草辅色
    auto WisL = d ? Theme::LilacLight     : Theme::LilacDeep;
    auto Sea  = d ? Theme::Mint           : Theme::MintDeep;     // 薄荷辅色
    auto SeaL = d ? Theme::MintLight      : Theme::MintDeep;
    auto SkyC = d ? Theme::Sky            : Theme::Sky;

    QString s;
    // ╔══════════════════════════════════════════════════════════╗
    // ║  NezhaGuard QSS · 男娘美学 · 草莓薄荷薰衣草             ║
    // ╚══════════════════════════════════════════════════════════╝

    // ── global ──
    s += QStringLiteral(
        "* { font-family:\"SF Pro Rounded\",\"PingFang SC\",system-ui,sans-serif; "
        "selection-background-color:") + Sel + QStringLiteral("; selection-color:") + Tx + QStringLiteral("; } "
        "QMainWindow { background:") + Bg + QStringLiteral("; } "
        "QLabel { color:") + Tx + QStringLiteral("; background:transparent; } ");

    // ── header · 草莓奶霜顶栏 ──
    s += QStringLiteral(
        "#header { background:") + Sh + QStringLiteral("; border-bottom:1px solid ") + Br + QStringLiteral("; "
        "min-height:48px; } "
        "#app_title { font-size:15px; font-weight:800; color:") + Pk + QStringLiteral("; letter-spacing:1px; } "
        "#brand_badge { background:transparent; border-radius:10px; } "
        "#clock_label { font-size:10px; color:") + Mu + QStringLiteral("; font-weight:500; } "
        "#status_text { font-size:10px; color:") + Sea + QStringLiteral("; font-weight:700; letter-spacing:0.5px; } "
        "#status_dot { background:") + Pk + QStringLiteral("; border-radius:5px; min-width:8px; max-width:8px; "
        "min-height:8px; max-height:8px; } ");

    // ── sidebar · 粉色导航 ──
    s += QStringLiteral(
        "#sidebar { background:") + Sh + QStringLiteral("; border-right:1px solid ") + Br + QStringLiteral("; "
        "padding:8px 0; } "
        "#sidebar::item { color:") + Mu + QStringLiteral("; padding:11px 24px; font-size:13px; font-weight:500; "
        "border:none; margin:2px 10px; border-radius:14px; } "
        "#sidebar::item:selected { background:") + Sel + QStringLiteral("; color:") + Pk + QStringLiteral("; "
        "font-weight:700; } "
        "#sidebar::item:hover:!selected { background:") + Hov + QStringLiteral("; color:") + Ros + QStringLiteral("; } ");

    // ── network card · 圆角卡片 ──
    s += QStringLiteral(
        "QFrame#netcard { background:") + Card + QStringLiteral("; border:1px solid ") + Br + QStringLiteral("; "
        "border-radius:20px; } ");

    // ── section titles ──
    s += QStringLiteral(
        "#dash_title,#logs_title,#alerts_title,#honey_title,#network_title { "
        "font-size:14px; font-weight:800; color:") + Tx + QStringLiteral("; padding-bottom:6px; } ");

    // ── stat cards · 四色左线 ──
    auto PeachAccent = d ? Theme::Peachy : Theme::PeachyDeep;
    s += QStringLiteral(
        "QFrame#card_logs,QFrame#card_alerts,QFrame#card_threats,QFrame#card_uptime { "
        "background:") + Card + QStringLiteral("; border:1px solid ") + Br + QStringLiteral("; "
        "border-radius:20px; padding:18px 24px; } "
        "QFrame#card_logs { border-left:4px solid ") + Pk + QStringLiteral("; } "
        "QFrame#card_alerts { border-left:4px solid ") + Theme::Coral + QStringLiteral("; } "
        "QFrame#card_threats { border-left:4px solid ") + PeachAccent + QStringLiteral("; } "
        "QFrame#card_uptime { border-left:4px solid ") + Sea + QStringLiteral("; } "
        "QFrame#card_logs:hover { background:") + Hov + QStringLiteral("; } "
        "QFrame#card_alerts:hover { background:") + Hov + QStringLiteral("; } "
        "QFrame#card_threats:hover { background:") + Hov + QStringLiteral("; } "
        "QFrame#card_uptime:hover { background:") + Hov + QStringLiteral("; } "
        "#card_logs_value,#card_alerts_value,#card_threats_value,#card_uptime_value { "
        "font-size:32px; font-weight:900; color:") + Tx + QStringLiteral("; } "
        "#card_logs_label,#card_alerts_label,#card_threats_label,#card_uptime_label { "
        "font-size:11px; font-weight:600; color:") + Mu + QStringLiteral("; letter-spacing:0.3px; } ");

    // ── tables · 圆角网格 ──
    s += QStringLiteral(
        "QTableView { background:") + Bg + QStringLiteral("; alternate-background-color:") + Card + QStringLiteral("; "
        "gridline-color:") + Br + QStringLiteral("; color:") + Tx + QStringLiteral("; "
        "border:1px solid ") + Br + QStringLiteral("; border-radius:16px; font-size:11px; "
        "selection-background-color:") + Sel + QStringLiteral("; selection-color:") + Tx + QStringLiteral("; } "
        "QTableView::item { padding:5px 10px; } "
        "QHeaderView::section { background:") + Card + QStringLiteral("; color:") + Mu + QStringLiteral("; "
        "border:none; border-bottom:1px solid ") + Br + QStringLiteral("; padding:6px 10px; "
        "font-size:10px; font-weight:700; letter-spacing:0.3px; } ");

    // ── combo ──
    s += QStringLiteral(
        "QComboBox { background:") + Card + QStringLiteral("; color:") + Tx + QStringLiteral("; "
        "border:1px solid ") + Br + QStringLiteral("; border-radius:14px; padding:7px 16px; "
        "font-size:11px; min-width:100px; } "
        "QComboBox:hover { border-color:") + Ros + QStringLiteral("; } "
        "QComboBox::drop-down { border:none; width:26px; subcontrol-origin:padding; "
        "subcontrol-position:top right; } "
        "QComboBox QAbstractItemView { background:") + Card + QStringLiteral("; color:") + Tx + QStringLiteral("; "
        "selection-background-color:") + Sel + QStringLiteral("; border:1px solid ") + Br + QStringLiteral("; "
        "border-radius:14px; padding:6px; outline:none; } ");

    // ── buttons · 圆角粉边 ──
    s += QStringLiteral(
        "QPushButton { background:") + Card + QStringLiteral("; color:") + Pk + QStringLiteral("; "
        "border:1px solid ") + Br + QStringLiteral("; border-radius:16px; padding:8px 22px; "
        "font-size:11px; font-weight:700; } "
        "QPushButton:hover { background:") + Sel + QStringLiteral("; border-color:") + Pk + QStringLiteral("; "
        "color:") + Pk + QStringLiteral("; } "
        "QPushButton:pressed { background:") + Hov + QStringLiteral("; } ");

    // ── inputs · 圆角输入 ──
    s += QStringLiteral(
        "QLineEdit { background:") + Bg + QStringLiteral("; color:") + Tx + QStringLiteral("; "
        "border:1px solid ") + Br + QStringLiteral("; border-radius:14px; padding:9px 16px; "
        "font-size:11px; } "
        "QLineEdit:focus { border-color:") + Pk + QStringLiteral("; } "
        "QTextEdit,QTableWidget { background:") + Bg + QStringLiteral("; color:") + Tx + QStringLiteral("; "
        "border:1px solid ") + Br + QStringLiteral("; border-radius:16px; font-size:11px; "
        "selection-background-color:") + Sel + QStringLiteral("; } "
        "QTextEdit:focus { border-color:") + Pk + QStringLiteral("; } "
        "QTableWidget::item:selected { background:") + Sel + QStringLiteral("; } "
        "QTableWidget::item:hover { background:") + Hov + QStringLiteral("; } ");

    // ── scrollbar · 超细粉条 ──
    s += QStringLiteral(
        "QScrollBar:vertical { background:transparent; width:4px; margin:2px; } "
        "QScrollBar::handle:vertical { background:") + BrH + QStringLiteral("; border-radius:2px; min-height:30px; } "
        "QScrollBar::handle:vertical:hover { background:") + Pk + QStringLiteral("; } "
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; width:0; } "
        "QScrollBar:horizontal { background:transparent; height:4px; margin:2px; } "
        "QScrollBar::handle:horizontal { background:") + BrH + QStringLiteral("; border-radius:2px; min-width:30px; } "
        "QScrollBar::handle:horizontal:hover { background:") + Pk + QStringLiteral("; } "
        "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal { width:0; } ");

    // ── tab widget ──
    s += QStringLiteral(
        "QTabWidget::pane { background:") + Card + QStringLiteral("; border:1px solid ") + Br + QStringLiteral("; "
        "border-radius:18px; padding:10px; } "
        "QTabBar::tab { background:") + Bg + QStringLiteral("; color:") + Mu + QStringLiteral("; "
        "padding:8px 18px; border:1px solid ") + Br + QStringLiteral("; border-bottom:none; "
        "border-top-left-radius:14px; border-top-right-radius:14px; font-size:11px; margin-right:2px; } "
        "QTabBar::tab:selected { background:") + Card + QStringLiteral("; color:") + Pk + QStringLiteral("; "
        "font-weight:700; } "
        "QTabBar::tab:hover:!selected { background:") + Hov + QStringLiteral("; color:") + Ros + QStringLiteral("; } "
        "QTabBar::scroller { width:22px; } "
        "QTabBar QToolButton { background:") + Card + QStringLiteral("; border:1px solid ") + Br + QStringLiteral("; "
        "border-radius:12px; color:") + Mu + QStringLiteral("; padding:3px; } "
        "QTabBar QToolButton:hover { color:") + Pk + QStringLiteral("; } ");

    // ── code editors ──
    s += QStringLiteral(
        "QPlainTextEdit { background:") + Bg + QStringLiteral("; color:") + Tx + QStringLiteral("; "
        "border:1px solid ") + Br + QStringLiteral("; border-radius:18px; "
        "font-family:\"SF Mono\",\"Menlo\",monospace; font-size:11px; padding:14px; "
        "selection-background-color:") + Sel + QStringLiteral("; } "
        "QPlainTextEdit:focus { border-color:") + Pk + QStringLiteral("; } ");

    // ── severity badges ──
    s += QStringLiteral(
        "#sev_crit,#sev_error,#sev_warn,#sev_info { border-radius:14px; font-weight:800; "
        "font-size:10px; padding:6px 0; background:") + Card + QStringLiteral("; "
        "border:1px solid ") + Br + QStringLiteral("; } "
        "#sev_crit { color:") + Theme::Coral + QStringLiteral("; } "
        "#sev_error { color:") + Theme::Tangerine + QStringLiteral("; } "
        "#sev_warn { color:") + Pk + QStringLiteral("; } "
        "#sev_info { color:") + Sea + QStringLiteral("; } ");

    // ── quick stats · 粉彩徽章 ──
    s += QStringLiteral(
        "#qs_tor,#qs_blocked,#qs_engines,#qs_types { border-radius:16px; font-size:11px; font-weight:700; "
        "padding:9px 16px; border:1px solid ") + Br + QStringLiteral("; } "
        "#qs_tor { background:") + (d ? QStringLiteral("rgba(242,152,176,0.14)") : QStringLiteral("rgba(232,120,148,0.12)")) + QStringLiteral("; color:") + Pk + QStringLiteral("; } "
        "#qs_blocked { background:") + (d ? QStringLiteral("rgba(240,104,128,0.14)") : QStringLiteral("rgba(240,104,128,0.12)")) + QStringLiteral("; color:") + Theme::Coral + QStringLiteral("; } "
        "#qs_engines { background:") + (d ? QStringLiteral("rgba(136,216,188,0.14)") : QStringLiteral("rgba(96,184,152,0.12)")) + QStringLiteral("; color:") + Sea + QStringLiteral("; } "
        "#qs_types { background:") + (d ? QStringLiteral("rgba(200,168,240,0.14)") : QStringLiteral("rgba(176,128,216,0.12)")) + QStringLiteral("; color:") + Wis + QStringLiteral("; } ");

    // ── settings ──
    s += QStringLiteral(
        "#settings_title { font-size:22px; font-weight:800; color:") + Pk + QStringLiteral("; } "
        "#settings_version { font-size:10px; color:") + Mu + QStringLiteral("; } "
        "#app_info_card { background:") + Card + QStringLiteral("; border:1px solid ") + Br + QStringLiteral("; "
        "border-radius:18px; } ");

    // ── status bar ──
    s += QStringLiteral(
        "QStatusBar { background:") + Sh + QStringLiteral("; border-top:1px solid ") + Br + QStringLiteral("; "
        "font-size:10px; color:") + Mu + QStringLiteral("; padding:4px 18px; } "
        "QStatusBar::item { border:none; } ");

    // ── tooltips · 圆角气泡 ──
    s += QStringLiteral(
        "QToolTip { background:") + Card + QStringLiteral("; color:") + Tx + QStringLiteral("; "
        "border:1px solid ") + BrH + QStringLiteral("; border-radius:16px; padding:10px 16px; font-size:11px; } ");

    // ── menus · 粉边弹出 ──
    s += QStringLiteral(
        "QMenuBar { background:") + Sh + QStringLiteral("; color:") + Tx + QStringLiteral("; "
        "border-bottom:1px solid ") + Br + QStringLiteral("; padding:2px 10px; font-size:11px; } "
        "QMenuBar::item:selected { background:") + Sel + QStringLiteral("; border-radius:10px; } "
        "QMenu { background:") + Card + QStringLiteral("; color:") + Tx + QStringLiteral("; "
        "border:1px solid ") + Br + QStringLiteral("; border-radius:18px; padding:8px; } "
        "QMenu::item { padding:9px 32px 9px 18px; border-radius:10px; font-size:11px; } "
        "QMenu::item:selected { background:") + Sel + QStringLiteral("; color:") + Pk + QStringLiteral("; } "
        "QMenu::separator { height:1px; background:") + Br + QStringLiteral("; margin:4px 10px; } ");

    setStyleSheet(s);
}

// -- setup --
void monitor::setup_sidebar() {
    const QString items[] = {
        QStringLiteral("仪表盘"),
        QStringLiteral("日志监控"),
        QStringLiteral("安全告警"),
        QStringLiteral("蜜罐监控"),
        QStringLiteral("网络信息"),
        QStringLiteral("系统配置")
    };
    ui->sidebar->clear();
    for (const auto &item : items)
        ui->sidebar->addItem(item);
    ui->sidebar->setCurrentRow(0);

    // 页面切换淡入动画
    connect(ui->sidebar, &QListWidget::currentRowChanged, this, &monitor::animate_page_switch);
}

void monitor::animate_page_switch(int row) {
    if (page_transitioning_ || row < 0 || row >= ui->pages->count()) return;
    if (ui->pages->currentIndex() == row) return;
    page_transitioning_ = true;

    auto *current_widget = ui->pages->currentWidget();
    auto *target_widget = ui->pages->widget(row);
    if (!current_widget || !target_widget) {
        ui->pages->setCurrentIndex(row);
        page_transitioning_ = false;
        return;
    }

    // fade out + slight scale down
    auto *out_fx = new QGraphicsOpacityEffect(this);
    current_widget->setGraphicsEffect(out_fx);
    auto *out_anim = new QPropertyAnimation(out_fx, "opacity", this);
    out_anim->setDuration(130);
    out_anim->setStartValue(1.0);
    out_anim->setEndValue(0.0);
    out_anim->setEasingCurve(QEasingCurve::InOutCubic);

    connect(out_anim, &QPropertyAnimation::finished, this, [=, this]() {
        current_widget->setGraphicsEffect(nullptr);
        ui->pages->setCurrentIndex(row);

        // fade in + slight slide up
        auto *in_fx = new QGraphicsOpacityEffect(this);
        target_widget->setGraphicsEffect(in_fx);
        auto *in_anim = new QPropertyAnimation(in_fx, "opacity", this);
        in_anim->setDuration(180);
        in_anim->setStartValue(0.0);
        in_anim->setEndValue(1.0);
        in_anim->setEasingCurve(QEasingCurve::OutCubic);

        connect(in_anim, &QPropertyAnimation::finished, this, [=, this]() {
            target_widget->setGraphicsEffect(nullptr);
            page_transitioning_ = false;
        });
        in_anim->start(QAbstractAnimation::DeleteWhenStopped);
    });
    out_anim->start(QAbstractAnimation::DeleteWhenStopped);

    if (row == 5) load_settings_configs();
}

void monitor::update_clock() {
    ui->clock_label->setText(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd  hh:mm:ss")));
}

void monitor::setup_log_table(QTableView *v) {
    v->setShowGrid(false);
    v->setAlternatingRowColors(false);
    v->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->setSelectionMode(QAbstractItemView::SingleSelection);
    v->verticalHeader()->setDefaultSectionSize(28);
    v->verticalHeader()->hide();
    v->horizontalHeader()->hide();
    v->setMouseTracking(true);
    v->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    v->setSortingEnabled(false);
    QScroller::grabGesture(v->viewport(), QScroller::LeftMouseButtonGesture);
}

void monitor::setup_network_table(QTableWidget *t) {
    t->verticalHeader()->hide();
    t->horizontalHeader()->setStretchLastSection(true);
    t->setAlternatingRowColors(true);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
}

void monitor::setup_network_page() {
    // helper: wrap a widget in a styled card with title
    auto makeCard = [this](const QString &title, QWidget *content, const QString &accentColor) -> QFrame* {
        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("netcard"));
        card->setFrameShape(QFrame::NoFrame);
        auto *lay = new QVBoxLayout(card);
        lay->setSpacing(8);
        lay->setContentsMargins(14, 12, 14, 12);

        auto *header = new QHBoxLayout();
        auto *dot = new QLabel(QStringLiteral("●"));
        dot->setStyleSheet(QStringLiteral("font-size:8px; color:") + accentColor + QStringLiteral("; background:transparent;"));
        auto *lbl = new QLabel(title);
        lbl->setStyleSheet(QStringLiteral(
            "font-size:11px; font-weight:700; color:") + (dark_mode_ ? Theme::DkText : Theme::LtText) +
            QStringLiteral("; background:transparent;"));
        header->addWidget(dot);
        header->addWidget(lbl);
        header->addStretch();
        lay->addLayout(header);
        lay->addWidget(content);
        return card;
    };

    // ── clear & rebuild page_network layout ──
    auto *page = ui->page_network;
    if (page->layout()) {
        QLayoutItem *child;
        while ((child = page->layout()->takeAt(0)) != nullptr) {
            // don't delete widgets, just detach them from the old layout
        }
        delete page->layout();
    }

    auto *root = new QVBoxLayout(page);
    root->setSpacing(14);
    root->setContentsMargins(22, 18, 22, 18);

    // ── header ──
    auto *hdr = new QHBoxLayout();
    ui->network_title->setStyleSheet(QStringLiteral("font-size:14px; font-weight:700; background:transparent;"));
    hdr->addWidget(ui->network_title);
    hdr->addStretch();
    ui->refresh_network->setText(QStringLiteral("刷新网络信息"));
    hdr->addWidget(ui->refresh_network);
    root->addLayout(hdr);

    // ── row 1: radar (left) + protocol stats (right) ──
    auto *row1 = new QHBoxLayout();
    row1->setSpacing(14);

    // radar
    radar_widget_ = new RadarWidget();
    radar_widget_->dark = dark_mode_;
    radar_widget_->setMinimumSize(360, 360);
    radar_widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *radarCard = makeCard(QStringLiteral("局域网设备扫描"),
                                radar_widget_,
                                dark_mode_ ? Theme::Seafoam : Theme::SeafoamDeep);
    row1->addWidget(radarCard, 2); // 2:1 ratio vs protocol stats

    // protocol stats
    protocol_table_ = new QTableWidget(0, 4, this);
    protocol_table_->setFrameShape(QFrame::NoFrame);
    protocol_table_->setAlternatingRowColors(true);
    protocol_table_->setShowGrid(false);
    protocol_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    protocol_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    protocol_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    protocol_table_->verticalHeader()->hide();
    protocol_table_->horizontalHeader()->setStretchLastSection(true);
    protocol_table_->setHorizontalHeaderLabels({
        QStringLiteral("协议"), QStringLiteral("包数"),
        QStringLiteral("字节"), QStringLiteral("占比")
    });
    protocol_table_->setMinimumHeight(360);
    setup_network_table(protocol_table_);
    auto *protoCard = makeCard(QStringLiteral("实时协议统计"),
                                protocol_table_,
                                dark_mode_ ? Theme::Wisteria : Theme::WisteriaDeep);
    row1->addWidget(protoCard, 1); // narrower column

    root->addLayout(row1);

    // ── row 2: local interfaces (full width) ──
    ui->local_ip_table->setMinimumHeight(120);
    ui->local_ip_table->setMaximumHeight(220);
    auto *localCard = makeCard(QStringLiteral("本地网络接口"),
                                ui->local_ip_table,
                                dark_mode_ ? Theme::Sakura : Theme::SakuraDeep);
    root->addWidget(localCard);

    // ── row 3: arp (left) + quarantine (right) ──
    auto *row3 = new QHBoxLayout();
    row3->setSpacing(14);

    ui->arp_table->setMinimumHeight(140);
    ui->arp_table->setMaximumHeight(260);
    auto *arpCard = makeCard(QStringLiteral("ARP 缓存表"),
                              ui->arp_table,
                              dark_mode_ ? Theme::Sky : Theme::Sky);
    row3->addWidget(arpCard, 1);

    ui->quarantine_table->setMinimumHeight(140);
    ui->quarantine_table->setMaximumHeight(260);
    auto *quarCard = makeCard(QStringLiteral("隔离列表"),
                               ui->quarantine_table,
                               dark_mode_ ? Theme::SakuraHot : Theme::SakuraHot);
    row3->addWidget(quarCard, 1);

    root->addLayout(row3);
    root->addStretch();
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
    log_proxy_->setDynamicSortFilter(true);
    log_proxy_->sort(0, Qt::DescendingOrder);

    setup_log_table(ui->log_view);
    ui->log_view->setModel(log_proxy_);
    ui->log_view->setColumnHidden(1, true);
    log_delegate_ = new LogDelegate(this);
    log_delegate_->dark = dark_mode_;
    ui->log_view->setItemDelegate(log_delegate_);
    ui->log_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->log_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(ui->log_view, &QTableView::clicked, this, &monitor::show_log_detail);
    connect(ui->level_filter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &monitor::apply_log_filter);

    log_search_proxy_ = new QSortFilterProxyModel(this);
    log_search_proxy_->setSourceModel(log_proxy_);
    log_search_proxy_->setFilterRole(LogModel::MessageRole);
    log_search_proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    log_search_proxy_->setSortRole(LogModel::TimestampRole);
    log_search_proxy_->setDynamicSortFilter(true);
    log_search_proxy_->sort(0, Qt::DescendingOrder);
    ui->log_view->setModel(log_search_proxy_);
    connect(ui->log_search_box, &QLineEdit::textChanged, this, &monitor::log_search_changed);
    connect(ui->log_clear_btn, &QPushButton::clicked, this, &monitor::clear_logs);

    // auto-scroll to latest (DESC = top)
    connect(log_model_, &QAbstractItemModel::rowsInserted, this, [this]() {
        QTimer::singleShot(0, this, [this]() {
            ui->log_view->scrollToTop();
        });
    });

    // alert page
    alert_model_ = new LogModel(this);
    alert_proxy_ = new QSortFilterProxyModel(this);
    alert_proxy_->setSourceModel(alert_model_);
    alert_proxy_->setFilterRole(LogModel::LevelRole);
    alert_proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    alert_proxy_->setSortRole(LogModel::TimestampRole);
    alert_proxy_->setDynamicSortFilter(true);
    alert_proxy_->sort(0, Qt::DescendingOrder);

    setup_log_table(ui->alert_view);
    ui->alert_view->setModel(alert_proxy_);
    alert_delegate_ = new AlertDelegate(this);
    alert_delegate_->dark = dark_mode_;
    ui->alert_view->setItemDelegate(alert_delegate_);
    ui->alert_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->alert_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(ui->alert_view, &QTableView::clicked, this, &monitor::show_alert_detail);
    connect(ui->alert_severity_filter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &monitor::filter_alert_severity);

    // dashboard recent alerts (DESC)
    recent_proxy_ = new QSortFilterProxyModel(this);
    recent_proxy_->setSourceModel(alert_model_);
    recent_proxy_->setSortRole(LogModel::TimestampRole);
    recent_proxy_->setDynamicSortFilter(true);
    recent_proxy_->sort(0, Qt::DescendingOrder);
    setup_log_table(ui->recent_alerts_view);
    ui->recent_alerts_view->setModel(recent_proxy_);
    recent_delegate_ = new AlertDelegate(this);
    recent_delegate_->dark = dark_mode_;
    ui->recent_alerts_view->setItemDelegate(recent_delegate_);
    connect(ui->recent_alerts_view, &QTableView::clicked, this, &monitor::show_alert_detail);

    // honeypot page (DESC)
    honey_model_ = new LogModel(this);
    honey_proxy_ = new QSortFilterProxyModel(this);
    honey_proxy_->setSourceModel(honey_model_);
    honey_proxy_->setSortRole(LogModel::TimestampRole);
    honey_proxy_->setDynamicSortFilter(true);
    honey_proxy_->sort(0, Qt::DescendingOrder);
    setup_log_table(ui->honey_view);
    ui->honey_view->setModel(honey_proxy_);
    honey_delegate_ = new LogDelegate(this);
    honey_delegate_->dark = dark_mode_;
    ui->honey_view->setItemDelegate(honey_delegate_);
    connect(ui->honey_view, &QTableView::clicked, this, &monitor::show_honey_detail);

    // network page
    setup_network_page();
    for (auto *t: {ui->local_ip_table, ui->arp_table, ui->quarantine_table})
        setup_network_table(t);

    // ARP 表点击查看详情
    connect(ui->arp_table, &QTableWidget::cellClicked, this, [this](int row, int) {
        auto *ipItem = ui->arp_table->item(row, 0);
        auto *macItem = ui->arp_table->item(row, 1);
        if (!ipItem || !macItem) return;
        QString ip = ipItem->text();
        QString mac = macItem->text();

        auto dlg = new QDialog(this);
        dlg->setWindowTitle(QStringLiteral("ARP 详情 — %1").arg(ip));
        dlg->resize(480, 340);
        dlg->setStyleSheet(QStringLiteral("QDialog { background:") +
            (dark_mode_ ? Theme::DkCard : Theme::LtCard) + QStringLiteral("; }"));

        auto *lay = new QVBoxLayout(dlg);
        lay->setContentsMargins(24, 20, 24, 20);
        lay->setSpacing(12);

        auto addRow = [&](const QString &k, const QString &v, const QString &c) {
            auto *row = new QHBoxLayout();
            auto *kl = new QLabel(k);
            kl->setFixedWidth(80);
            kl->setStyleSheet(QStringLiteral("font-size:11px; font-weight:700; color:") +
                (dark_mode_ ? Theme::DkMuted : Theme::LtMuted) + QStringLiteral("; background:transparent;"));
            auto *vl = new QLabel(v);
            vl->setWordWrap(true);
            vl->setTextInteractionFlags(Qt::TextSelectableByMouse);
            vl->setStyleSheet(QStringLiteral("font-size:12px; font-weight:600; color:") + c +
                QStringLiteral("; background:transparent; font-family:\"SF Mono\",\"Menlo\",monospace;"));
            row->addWidget(kl);
            row->addWidget(vl, 1);
            lay->addLayout(row);
        };

        auto accent = dark_mode_ ? Theme::Strawberry : Theme::RosyDeep;
        auto text   = dark_mode_ ? Theme::DkText : Theme::LtText;
        auto mint   = dark_mode_ ? Theme::Mint : Theme::MintDeep;

        addRow(QStringLiteral("IP 地址"), ip, accent);
        addRow(QStringLiteral("MAC 地址"), mac, text);

        // OUI vendor lookup from MAC prefix
        QString oui = mac.left(8).toUpper();
        // common OUIs
        static QHash<QString,QString> vendors = {
            {"BC:92:6B","Apple"}, {"AC:BC:32","Apple"}, {"F0:18:98","Apple"},
            {"B8:27:EB","Raspberry Pi"}, {"DC:A6:32","Raspberry Pi"},
            {"00:1A:11","Google"}, {"3C:5A:B4","Google"},
            {"F4:F5:D8","Samsung"}, {"00:1E:C2","Samsung"},
            {"00:25:9C","Cisco"}, {"00:1B:53","Cisco"},
            {"00:0C:29","VMware"}, {"00:50:56","VMware"},
            {"08:00:27","VirtualBox"},
            {"D4:61:DA","Xiaomi"}, {"F4:DB:E3","Xiaomi"},
            {"58:CB:52","Huawei"}, {"00:1E:10","Huawei"},
            {"A4:50:46","Intel"}, {"00:1F:3B","Intel"},
            {"B0:6E:BF","Dell"},
            {"70:4D:7B","ASUS"},
            {"48:21:0B","Nintendo"},
            {"00:04:4B","NVIDIA"},
        };
        QString vendor = vendors.value(oui, QStringLiteral("未知厂商"));
        addRow(QStringLiteral("厂商"), vendor, mint);

        addRow(QStringLiteral("OUI"), oui, dark_mode_ ? Theme::DkMuted : Theme::LtMuted);

        auto *btn = new QPushButton(QStringLiteral("关闭"));
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background:transparent; color:") + accent + QStringLiteral("; "
            "border:1px solid ") + (dark_mode_ ? Theme::DkBorder : Theme::LtBorder) + QStringLiteral("; "
            "border-radius:14px; padding:8px 24px; font-size:11px; font-weight:600; } "
            "QPushButton:hover { background:") + (dark_mode_ ? Theme::DkSelected : Theme::LtSelected) +
            QStringLiteral("; }"));
        connect(btn, &QPushButton::clicked, dlg, &QDialog::close);
        auto *btnRow = new QHBoxLayout();
        btnRow->addStretch();
        btnRow->addWidget(btn);
        lay->addLayout(btnRow);

        dlg->show();
    });

    // 隔离列表点击查看详情
    connect(ui->quarantine_table, &QTableWidget::cellClicked, this, [this](int row, int) {
        auto *ipItem = ui->quarantine_table->item(row, 0);
        auto *reasonItem = ui->quarantine_table->item(row, 1);
        auto *scoreItem = ui->quarantine_table->item(row, 2);
        if (!ipItem) return;
        QString ip = ipItem->text();
        QString reason = reasonItem ? reasonItem->text() : QStringLiteral("-");
        QString score = scoreItem ? scoreItem->text() : QStringLiteral("0");

        auto dlg = new QDialog(this);
        dlg->setWindowTitle(QStringLiteral("隔离详情 — %1").arg(ip));
        dlg->resize(420, 280);
        dlg->setStyleSheet(QStringLiteral("QDialog { background:") +
            (dark_mode_ ? Theme::DkCard : Theme::LtCard) + QStringLiteral("; }"));

        auto *lay = new QVBoxLayout(dlg);
        lay->setContentsMargins(24, 20, 24, 20);
        lay->setSpacing(12);

        auto addRow = [&](const QString &k, const QString &v, const QString &c) {
            auto *r = new QHBoxLayout();
            auto *kl = new QLabel(k);
            kl->setFixedWidth(80);
            kl->setStyleSheet(QStringLiteral("font-size:11px; font-weight:700; color:") +
                (dark_mode_ ? Theme::DkMuted : Theme::LtMuted) + QStringLiteral("; background:transparent;"));
            auto *vl = new QLabel(v);
            vl->setWordWrap(true);
            vl->setTextInteractionFlags(Qt::TextSelectableByMouse);
            vl->setStyleSheet(QStringLiteral("font-size:12px; font-weight:600; color:") + c +
                QStringLiteral("; background:transparent; font-family:\"SF Mono\",\"Menlo\",monospace;"));
            r->addWidget(kl);
            r->addWidget(vl, 1);
            lay->addLayout(r);
        };

        auto coral  = Theme::Coral;
        auto text   = dark_mode_ ? Theme::DkText : Theme::LtText;
        auto muted  = dark_mode_ ? Theme::DkMuted : Theme::LtMuted;

        addRow(QStringLiteral("IP 地址"), ip, coral);
        addRow(QStringLiteral("隔离原因"), reason, text);
        addRow(QStringLiteral("威胁评分"), score, coral);

        auto *btnRow = new QHBoxLayout();
        btnRow->addStretch();
        auto *unqBtn = new QPushButton(QStringLiteral("取消隔离"));
        unqBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background:transparent; color:") + coral + QStringLiteral("; "
            "border:1px solid ") + coral + QStringLiteral("; border-radius:14px; padding:8px 20px; "
            "font-size:11px; font-weight:600; } "
            "QPushButton:hover { background:rgba(240,104,128,0.12); }"));
        connect(unqBtn, &QPushButton::clicked, dlg, [dlg, ip, this]() {
            Nezha::Database::DatabaseHelper::RemoveQuarantine(ip.toStdString());
            refresh_quarantine_list();
            dlg->close();
        });
        btnRow->addWidget(unqBtn);

        auto *closeBtn = new QPushButton(QStringLiteral("关闭"));
        auto accent = dark_mode_ ? Theme::Strawberry : Theme::RosyDeep;
        closeBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background:transparent; color:") + accent + QStringLiteral("; "
            "border:1px solid ") + (dark_mode_ ? Theme::DkBorder : Theme::LtBorder) +
            QStringLiteral("; border-radius:14px; padding:8px 20px; font-size:11px; font-weight:600; } "
            "QPushButton:hover { background:") + (dark_mode_ ? Theme::DkSelected : Theme::LtSelected) +
            QStringLiteral("; }"));
        connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);
        btnRow->addWidget(closeBtn);
        lay->addLayout(btnRow);

        dlg->show();
    });

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
    netTimer->start(10000);

    // sparkline
    sparkline_widget_ = new SparklineWidget();
    sparkline_widget_->dark = dark_mode_;
    if (ui->sparkline_frame->layout()) delete ui->sparkline_frame->layout();
    auto *vbl = new QVBoxLayout(ui->sparkline_frame);
    vbl->setContentsMargins(0, 0, 0, 0);
    vbl->addWidget(sparkline_widget_);

    // detail panels
    auto replace = [this](QTextEdit *old, DetailPanel *&panel, int maxH) {
        auto *pl = qobject_cast<QVBoxLayout *>(old->parentWidget()->layout());
        if (!pl) return;
        int i = pl->indexOf(old);
        old->hide();
        panel = new DetailPanel(this);
        panel->setMaximumHeight(maxH);
        panel->set_dark(dark_mode_);
        if (i >= 0) pl->insertWidget(i, panel);
    };
    replace(ui->log_detail, log_detail_panel_, 180);
    replace(ui->alert_detail, alert_detail_panel_, 110);
    replace(ui->honey_detail, honey_detail_panel_, 110);

    // context menus
    auto ctx = [this](const QPoint &pos, QTableView *v, QSortFilterProxyModel *proxy) {
        auto idx = v->indexAt(pos);
        if (!idx.isValid()) return;
        QMenu m(this);
        auto *cpy = m.addAction(QStringLiteral("复制"));
        auto *qip = m.addAction(QStringLiteral("隔离此 IP"));
        if (m.exec(v->viewport()->mapToGlobal(pos)) == cpy)
            QApplication::clipboard()->setText(proxy
                                                   ? proxy->data(idx.siblingAtColumn(2), LogModel::MessageRole).
                                                   toString()
                                                   : idx.data(LogModel::MessageRole).toString());
        else if (qip) {
            QRegularExpression rx(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");
            auto ip = rx.match(proxy
                                   ? proxy->data(idx.siblingAtColumn(2), LogModel::MessageRole).toString()
                                   : idx.data(LogModel::MessageRole).toString()).captured(1);
            if (!ip.isEmpty()) {
                Nezha::Database::DatabaseHelper::QuarantineIP(ip.toStdString(), "手动隔离", 50.0);
                refresh_quarantine_list();
            }
        }
    };

    for (auto &[view, proxy]: {std::pair{ui->log_view, log_search_proxy_}, {ui->alert_view, alert_proxy_}}) {
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view, &QTableView::customContextMenuRequested, this, [this, view, proxy, ctx](const QPoint &p) {
            ctx(p, view, proxy);
        });
    }

    ui->honey_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->honey_view, &QTableView::customContextMenuRequested, this, [this](const QPoint &pos) {
        auto idx = ui->honey_view->indexAt(pos);
        if (!idx.isValid()) return;

        QMenu m(this);
        m.addAction(QStringLiteral("复制"));

        if (auto *qip = m.addAction(QStringLiteral("隔离来源 IP")); m.exec(ui->honey_view->viewport()->mapToGlobal(pos)) == qip) {
            QRegularExpression rx(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");

            auto ip = rx.match(idx.data(LogModel::MessageRole).toString()).captured(1);

            if (!ip.isEmpty()) {
                Nezha::Database::DatabaseHelper::QuarantineIP(ip.toStdString(), "蜜罐手动隔离", 75.0);
                refresh_quarantine_list();
            }
        }
    });

    ui->quarantine_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->quarantine_table, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        auto *item = ui->quarantine_table->itemAt(pos);
        if (!item) return;

        const auto ip = ui->quarantine_table->item(item->row(), 0)->text();

        QMenu m(this);
        m.addAction(QStringLiteral("复制 IP"));

        auto *unq = m.addAction(QStringLiteral("取消隔离"));

        if (m.exec(ui->quarantine_table->viewport()->mapToGlobal(pos)) == unq) {
            Nezha::Database::DatabaseHelper::RemoveQuarantine(ip.toStdString());
            refresh_quarantine_list();
        }
    });

    // double-click → comprehensive detail dialog
    auto double_click_detail = [this](const QModelIndex &idx, LogModel *model) {
        if (!idx.isValid() || !model) return;
        auto tm = idx.data(LogModel::TimestampRole).toString();
        auto lv = idx.data(LogModel::LevelRole).toString();
        auto msg = idx.data(LogModel::MessageRole).toString();
        auto color = idx.data(LogModel::ColorRole).value<QColor>();

        QRegularExpression iprx(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");
        auto ipm = iprx.match(msg);
        QString ip = ipm.hasMatch() ? ipm.captured(1) : QString();

        auto text = dark_mode_ ? Theme::DkText : Theme::LtText;
        auto muted = dark_mode_ ? Theme::DkMuted : Theme::LtMuted;
        auto bg = dark_mode_ ? Theme::DkBg : Theme::LtBg;
        auto card = dark_mode_ ? Theme::DkCard : Theme::LtCard;
        auto border = dark_mode_ ? Theme::DkBorder : Theme::LtBorder;

        auto *dlg = new QDialog(this);
        dlg->setWindowTitle(QStringLiteral("告警详情 — %1").arg(ip.isEmpty() ? QStringLiteral("?") : ip));
        dlg->resize(620, 520);
        dlg->setStyleSheet(QStringLiteral(
            "QDialog { background:%1; }"
        ).arg(bg));

        auto *root = new QVBoxLayout(dlg);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(6);

        // -- detail table (wireshark-style selectable rows) --
        auto *tree = new QTreeWidget(dlg);
        tree->setHeaderLabels({QStringLiteral("字段"), QStringLiteral("值")});
        tree->setRootIsDecorated(true);
        tree->setAlternatingRowColors(false);
        tree->setAnimated(true);
        tree->setStyleSheet(QStringLiteral(
            "QTreeWidget { background:%1; color:%2; border:1px solid %3; border-radius:8px;"
            "  font-family:\"Menlo\"; font-size:11px; outline:none; }"
            "QTreeWidget::item { padding:3px 6px; border-bottom:1px solid %3; }"
            "QTreeWidget::item:hover { background:%4; }"
            "QTreeWidget::item:selected { background:%5; color:%6; }"
            "QHeaderView::section { background:%7; color:%8; padding:5px 10px;"
            "  border:none; border-bottom:2px solid %3; font-size:10px; font-weight:700; }"
        ).arg(card, text, border, dark_mode_ ? Theme::DkHover : Theme::LtHover,
              dark_mode_ ? Theme::DkSelected : Theme::LtSelected,
              dark_mode_ ? Theme::Cyan : Theme::CyanDeep,
              card, muted));

        auto add_row = [&](QTreeWidgetItem *parent, const QString &k, const QString &v,
                           const QString &vc = QString()) {
            auto *item = new QTreeWidgetItem(parent);
            item->setText(0, k);
            item->setText(1, v);
            if (!vc.isEmpty()) item->setForeground(1, QColor(vc));
        };

        auto *rootItem = tree->invisibleRootItem();
        add_row(rootItem, QStringLiteral("时间"), tm, text);
        add_row(rootItem, QStringLiteral("级别"), lv, color.name());
        add_row(rootItem, QStringLiteral("内容"), msg, text);

        // hex dump as expandable subtree
        auto *hexItem = new QTreeWidgetItem(rootItem);
        hexItem->setText(0, QStringLiteral("Hex Dump"));
        hexItem->setText(1, QStringLiteral("%1 字节").arg(msg.toUtf8().size()));
        hexItem->setForeground(1, QColor(muted));

        QByteArray raw = msg.toUtf8();
        for (int i = 0; i < raw.size(); i += 16) {
            QString hex, asc;
            for (int j = 0; j < 16; ++j) {
                if (i + j < raw.size()) {
                    unsigned char c = raw[i + j];
                    hex += QStringLiteral("%1 ").arg(c, 2, 16, QChar('0'));
                    asc += (c >= 32 && c < 127) ? QChar(c) : QChar('.');
                } else {
                    hex += QStringLiteral("   ");
                    asc += QChar(' ');
                }
            }
            auto *row = new QTreeWidgetItem(hexItem);
            row->setText(0, QStringLiteral("%1").arg(i, 4, 16, QChar('0')));
            row->setText(1, hex.trimmed() + QStringLiteral("  |  ") + asc);
            row->setForeground(1, QColor(Theme::Grey));
        }

        auto *geoItem = new QTreeWidgetItem(rootItem);
        geoItem->setText(0, QStringLiteral("GeoIP"));
        geoItem->setText(1, QStringLiteral("查询中..."));
        geoItem->setForeground(1, QColor(muted));

        tree->expandAll();
        tree->resizeColumnToContents(0);
        root->addWidget(tree);

        // button bar
        auto *btnBar = new QHBoxLayout();
        btnBar->setSpacing(8);
        root->addLayout(btnBar);

        auto mkbtn = [&](const QString &label, const QString &color) -> QPushButton * {
            auto *b = new QPushButton(label);
            b->setStyleSheet(QStringLiteral(
                "QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:8px;"
                "  padding:5px 16px; font-size:10px; font-weight:600; }"
                "QPushButton:hover { border-color:%2; background:%4; }"
            ).arg(card, color, border, dark_mode_ ? Theme::DkHover : Theme::LtHover));
            return b;
        };

        auto *copyBtn = mkbtn(QStringLiteral("复制全部"), dark_mode_ ? Theme::Cyan : Theme::CyanDeep);
        connect(copyBtn, &QPushButton::clicked, dlg, [tree, msg, tm, lv]() {
            QString all;
            all += QStringLiteral("时间: %1\n级别: %2\n内容: %3\n\n").arg(tm, lv, msg);
            all += QStringLiteral("── Hex Dump ──\n");
            QByteArray raw = msg.toUtf8();
            for (int i = 0; i < raw.size(); i += 16) {
                all += QStringLiteral("%1  ").arg(i, 4, 16, QChar('0'));
                for (int j = 0; j < 16 && (i + j) < raw.size(); ++j)
                    all += QStringLiteral("%1 ").arg(static_cast<unsigned char>(raw[i + j]), 2, 16, QChar('0'));
                all += QStringLiteral("\n");
            }
            QApplication::clipboard()->setText(all);
        });
        btnBar->addWidget(copyBtn);

        auto *copyJsonBtn = mkbtn(QStringLiteral("复制 JSON"), Theme::Pink);
        connect(copyJsonBtn, &QPushButton::clicked, dlg, [msg, tm, lv]() {
            QJsonObject obj;
            obj["timestamp"] = tm;
            obj["level"] = lv;
            obj["message"] = msg;
            obj["hex"] = QString::fromLatin1(msg.toUtf8().toHex());
            QApplication::clipboard()->setText(
                QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented)));
        });
        btnBar->addWidget(copyJsonBtn);

        if (!ip.isEmpty()) {
            auto *nmapBtn = mkbtn(QStringLiteral("Nmap 扫描"), Theme::Purple);
            connect(nmapBtn, &QPushButton::clicked, dlg, [this, ip]() {
                run_nmap_scan(ip);
            });
            btnBar->addWidget(nmapBtn);
        }

        auto *exportBtn = mkbtn(QStringLiteral("导出 .log"), Theme::Green);
        connect(exportBtn, &QPushButton::clicked, dlg, [dlg, msg, tm, lv, ip]() {
            QString path = QFileDialog::getSaveFileName(dlg, QStringLiteral("导出日志"),
                                                        QStringLiteral("alert_%1.log").arg(
                                                            QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
                                                        QStringLiteral("日志文件 (*.log);;所有文件 (*)"));
            if (path.isEmpty()) return;
            QFile f(path);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream ts(&f);
                ts << QStringLiteral("=== NezhaGuard 告警详情 ===\n");
                ts << QStringLiteral("时间: %1\n").arg(tm);
                ts << QStringLiteral("级别: %2\n").arg(lv);
                ts << QStringLiteral("来源: %3\n").arg(ip);
                ts << QStringLiteral("内容: %1\n\n").arg(msg);
                ts << QStringLiteral("── Hex Dump ──\n");
                QByteArray raw = msg.toUtf8();
                for (int i = 0; i < raw.size(); i += 16) {
                    ts << QStringLiteral("%1  ").arg(i, 4, 16, QChar('0'));
                    for (int j = 0; j < 16 && (i + j) < raw.size(); ++j)
                        ts << QStringLiteral("%1 ").arg(static_cast<unsigned char>(raw[i + j]), 2, 16, QChar('0'));
                    ts << QStringLiteral("  ");
                    for (int j = 0; j < 16 && (i + j) < raw.size(); ++j) {
                        unsigned char c = raw[i + j];
                        ts << ((c >= 32 && c < 127) ? QChar(c) : QChar('.'));
                    }
                    ts << QStringLiteral("\n");
                }
                f.close();
            }
        });
        btnBar->addWidget(exportBtn);

        btnBar->addStretch();

        auto *closeBtn = mkbtn(QStringLiteral("关闭"), muted);
        connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);
        btnBar->addWidget(closeBtn);

        // tree context menu
        tree->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(tree, &QTreeWidget::customContextMenuRequested, dlg,
                [tree](const QPoint &pos) {
                    auto *item = tree->itemAt(pos);
                    if (!item) return;
                    QMenu menu;
                    auto *cpy = menu.addAction(QStringLiteral("复制行"));
                    auto *cpyAll = menu.addAction(QStringLiteral("复制子树"));
                    if (menu.exec(tree->viewport()->mapToGlobal(pos)) == cpy) {
                        QApplication::clipboard()->setText(
                            QStringLiteral("%1: %2").arg(item->text(0), item->text(1)));
                    } else if (cpyAll) {
                        QString all;
                        std::function<void(QTreeWidgetItem *, int)> walk = [&](QTreeWidgetItem *it, int depth) {
                            all += QString(depth * 2, ' ') + it->text(0) + ": " + it->text(1) + "\n";
                            for (int i = 0; i < it->childCount(); ++i) walk(it->child(i), depth + 1);
                        };
                        walk(item, 0);
                        QApplication::clipboard()->setText(all);
                    }
                });

        // async GeoIP
        if (!ip.isEmpty()) {
            auto _f = QtConcurrent::run([this, geoItem, ip, muted, text]() {
                auto geo = Nezha::Core::GeoIP::lookup(ip.toStdString());
                auto host = Nezha::IPAddress::ipaddr::ResolveHostname(ip.toStdString());
                QMetaObject::invokeMethod(this, [geoItem, geo, host, muted, text, ip]() {
                    while (geoItem->childCount() > 0)
                        delete geoItem->takeChild(0);
                    auto add = [&](const QString &k, const QString &v, const QString &c = QString()) {
                        auto *item = new QTreeWidgetItem(geoItem);
                        item->setText(0, k);
                        item->setText(1, v);
                        if (!c.isEmpty()) item->setForeground(1, QColor(c));
                    };
                    geoItem->setText(1, QString());
                    if (!host.empty() && host != ip.toStdString())
                        add(QStringLiteral("主机名"), QString::fromStdString(host), text);
                    if (geo.valid) {
                        add(QStringLiteral("国家"), QStringLiteral("%1 (%2)")
                            .arg(QString::fromStdString(geo.country), QString::fromStdString(geo.country_code)), text);
                        if (!geo.city.empty()) add(QStringLiteral("城市"), QString::fromStdString(geo.city), text);
                        if (!geo.isp.empty()) add(QStringLiteral("ISP"), QString::fromStdString(geo.isp), text);
                        if (geo.lat != 0.0 || geo.lon != 0.0)
                            add(QStringLiteral("坐标"), QStringLiteral("%1, %2").arg(geo.lat, 0, 'f', 4).arg(
                                    geo.lon, 0, 'f', 4), text);
                    } else {
                        add(QStringLiteral("结果"), QStringLiteral("无数据"), muted);
                    }
                }, Qt::QueuedConnection);
            });
        }

        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    };

    connect(ui->recent_alerts_view, &QAbstractItemView::doubleClicked, this,
            [this, double_click_detail](const QModelIndex &idx) {
                double_click_detail(idx, alert_model_);
            });
    connect(ui->attackers_view, &QAbstractItemView::doubleClicked, this,
            [this, double_click_detail](const QModelIndex &idx) {
                double_click_detail(idx, attackers_model_);
            });
    connect(ui->alert_view, &QAbstractItemView::doubleClicked, this,
            [this, double_click_detail](const QModelIndex &idx) {
                auto srcIdx = alert_proxy_ ? alert_proxy_->mapToSource(idx) : idx;
                double_click_detail(srcIdx, alert_model_);
            });
    connect(ui->log_view, &QAbstractItemView::doubleClicked, this,
            [this, double_click_detail](const QModelIndex &idx) {
                auto srcIdx = log_search_proxy_ ? log_search_proxy_->mapToSource(idx) : idx;
                double_click_detail(srcIdx, log_model_);
            });
    auto *log_enter = new QShortcut(QKeySequence(Qt::Key_Return), ui->log_view, nullptr, nullptr, Qt::WidgetShortcut);
    connect(log_enter, &QShortcut::activated, this, [this, double_click_detail]() {
        auto idx = ui->log_view->currentIndex();
        if (idx.isValid()) {
            auto srcIdx = log_search_proxy_ ? log_search_proxy_->mapToSource(idx) : idx;
            double_click_detail(srcIdx, log_model_);
        }
    });
    auto *alert_enter = new QShortcut(QKeySequence(Qt::Key_Return), ui->alert_view, nullptr, nullptr, Qt::WidgetShortcut);
    connect(alert_enter, &QShortcut::activated, this, [this, double_click_detail]() {
        auto idx = ui->alert_view->currentIndex();
        if (idx.isValid()) {
            auto srcIdx = alert_proxy_ ? alert_proxy_->mapToSource(idx) : idx;
            double_click_detail(srcIdx, alert_model_);
        }
    });
    auto *recent_enter = new QShortcut(QKeySequence(Qt::Key_Return), ui->recent_alerts_view, nullptr, nullptr, Qt::WidgetShortcut);
    connect(recent_enter, &QShortcut::activated, this, [this, double_click_detail]() {
        double_click_detail(ui->recent_alerts_view->currentIndex(), alert_model_);
    });
    auto *honey_enter = new QShortcut(QKeySequence(Qt::Key_Return), ui->honey_view, nullptr, nullptr, Qt::WidgetShortcut);
    connect(honey_enter, &QShortcut::activated, this, [this, double_click_detail]() {
        double_click_detail(ui->honey_view->currentIndex(), honey_model_);
    });

    apply_theme(dark_mode_);
}

// -- stats --
static void animate_label_int(QLabel *label, int from, int to) {
    if (!label) return;
    // stop any previous animation on this label
    for (auto *child : label->children()) {
        if (auto *a = qobject_cast<QVariantAnimation *>(child))
            a->stop();
    }
    if (from == to) {
        label->setText(QString::number(to));
        return;
    }
    auto *anim = new QVariantAnimation(label);
    anim->setDuration(280);
    anim->setStartValue(from);
    anim->setEndValue(to);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(anim, &QVariantAnimation::valueChanged, label, [label](const QVariant &v) {
        label->setText(QString::number(v.toInt()));
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void monitor::update_stats(int log_count, int alert_count) {
    static int last_log = 0, last_alert = 0;
    static int cached_qc = -1;
    static int qc_refresh_counter = 0;
    static int prev_log = 0, prev_alert = 0, prev_qc = 0;
    int logRate = std::max(0, log_count - last_log);
    int alertRate = std::max(0, alert_count - last_alert);
    pkt_rate_ = logRate;
    last_log = log_count;
    last_alert = alert_count;
    if (cached_qc < 0 || ++qc_refresh_counter >= 5) {
        cached_qc = static_cast<int>(Nezha::Database::DatabaseHelper::GetQuarantineList().size());
        qc_refresh_counter = 0;
    }
    int qc = cached_qc;

    animate_label_int(ui->card_logs_value, prev_log, log_count);
    animate_label_int(ui->card_alerts_value, prev_alert, alert_count);
    animate_label_int(ui->card_threats_value, prev_qc, qc);
    prev_log = log_count;
    prev_alert = alert_count;
    prev_qc = qc;

    auto threatLvl = alertRate > 10
                         ? QStringLiteral("危险")
                         : alertRate > 3
                               ? QStringLiteral("警告")
                               : QStringLiteral("正常");
    auto threatColor = alertRate > 10 ? Theme::Red : alertRate > 3 ? Theme::Orange : Theme::Green;
    ui->status_label->setText(QStringLiteral("威胁: %1 | 日志 %2 (+%3/s) | 告警 %4 (+%5/s) | 已隔离 %6")
        .arg(threatLvl).arg(log_count).arg(logRate).arg(alert_count).arg(alertRate).arg(qc));

    ui->card_logs_label->setText(QStringLiteral("日志  +%1/s").arg(logRate));
    ui->card_alerts_label->setText(QStringLiteral("告警  +%1/s").arg(alertRate));
    ui->card_threats_label->setText(QStringLiteral("隔离  |  %1 种攻击").arg(active_types_.size()));

    int secs = std::max(1, start_time_.secsTo(QTime::currentTime()));
    int h = secs / 3600, m = (secs % 3600) / 60;
    ui->card_uptime_value->setText(QStringLiteral("%1h %2m").arg(h).arg(m));
    ui->card_uptime_label->setText(QStringLiteral("运行  |  %1 攻击者").arg(attackers_.size()));
    refresh_quickstats();
}

void monitor::append_alert(const QString &time, const QString &type, const QString &ip, int count, double score,
                           const QString &severity) {
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
    for (const auto &t: active_types_) {
        auto color = sev_crit_ > 0 ? Theme::PinkDeep : Theme::Pink;
        types.append(
            QStringLiteral(
                "<span style='color:%1;background:%2;border-radius:12px;padding:2px 10px;margin:1px;font-size:10px;font-weight:600;'>%3</span>")
            .arg(Theme::White, Theme::DkCard, t));
    }
    ui->threat_activity->setText(types.isEmpty() ? QString() : types.join(QStringLiteral(" ")));
    ui->threat_activity->setTextFormat(Qt::RichText);
}

void monitor::append_honeypot(const QString &time, const QString &src_ip, uint16_t sport, uint16_t dport,
                              const QString &service) {
    if (!honey_model_) return;
    QString msg = sport > 0
        ? QStringLiteral("%1:%2 → 端口 %3 [%4]").arg(src_ip).arg(sport).arg(dport).arg(service)
        : QStringLiteral("%1 → 端口 %2 [%3]").arg(src_ip).arg(dport).arg(service);
    honey_model_->append(time, QStringLiteral("蜜罐"), msg);
    ui->honey_stats_label->setText(QStringLiteral("共 %1 次连接").arg(honey_model_->total()));
}

void monitor::record_attacker(const QString &ip, double score, const QString &type) {
    auto &a = attackers_[ip];
    a.score = std::max(a.score, score);
    a.count++;
    if (!type.isEmpty()) a.type = type;

    // cap attackers map: evict lowest-score entries when over limit
    constexpr int kMaxAttackers = 200;
    if (attackers_.size() > kMaxAttackers) {
        QString worst;
        double worstVal = 1e18;
        for (auto it = attackers_.cbegin(); it != attackers_.cend(); ++it) {
            double v = it->score * it->count;
            if (v < worstVal) { worstVal = v; worst = it.key(); }
        }
        if (!worst.isEmpty()) attackers_.remove(worst);
    }

    // debounce model rebuild: batch every 200ms to avoid O(n log n) per alert
    static QElapsedTimer lastRebuild;
    if (lastRebuild.isValid() && lastRebuild.elapsed() < 200) return;
    lastRebuild.start();

    // throttle with a single-shot timer to coalesce rapid-fire alerts
    if (attackers_model_) {
        QTimer::singleShot(50, this, [this]() { refresh_attackers(); });
    }
}

void monitor::refresh_attackers() {
    if (!attackers_model_) return;
    attackers_model_->clear();

    QList<std::pair<QString, Attacker>> sorted;
    for (auto it = attackers_.cbegin(); it != attackers_.cend(); ++it)
        sorted.append({it.key(), it.value()});
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
        return a.second.score * a.second.count > b.second.score * b.second.count;
    });

    int rank = 0;
    for (const auto &[ip, a]: sorted) {
        if (++rank > 10) break;
        attackers_model_->append(
            QStringLiteral("#%1").arg(rank),
            a.score > 80 ? QStringLiteral("CRIT") : a.score > 50 ? QStringLiteral("WARN") : QStringLiteral("INFO"),
            QStringLiteral("%1  |  %2分  x%3  [%4]").arg(ip).arg(static_cast<int>(a.score)).arg(a.count).arg(a.type));
    }
}

// -- filters --
void monitor::apply_log_filter(int idx) {
    if (log_proxy_) log_proxy_->setFilterFixedString(idx == 0 ? QString() : ui->level_filter->currentText());
}

void monitor::filter_alert_severity(int idx) {
    if (alert_proxy_) alert_proxy_->setFilterFixedString(
        idx == 0 ? QString() : ui->alert_severity_filter->currentText());
}

void monitor::log_search_changed(const QString &t) {
    if (log_search_proxy_) log_search_proxy_->setFilterFixedString(t);
}

// -- detail --
void monitor::show_log_detail(const QModelIndex &idx) {
    if (!idx.isValid() || !log_detail_panel_) return;
    auto tm = idx.data(LogModel::TimestampRole).toString();
    auto lv = idx.data(LogModel::LevelRole).toString();
    auto msg = idx.data(LogModel::MessageRole).toString();
    QRegularExpression rx(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");
    QStringList ips;
    auto it = rx.globalMatch(msg);
    while (it.hasNext()) {
        auto ip = it.next().captured(1);
        if (!ips.contains(ip)) ips.append(ip);
    }
    log_detail_panel_->show_log(tm, lv, msg, ips);
    if (!ips.isEmpty()) {
        auto ips2 = ips;
        auto _f = QtConcurrent::run([this, ips2]() {
            for (const auto &ip: ips2) {
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
    auto *t = ui->local_ip_table;
    t->setRowCount(0);
    t->setHorizontalHeaderLabels({
        QStringLiteral(""), QStringLiteral("接口"),
        QStringLiteral("IP 地址"), QStringLiteral("子网掩码"),
        QStringLiteral("类型")
    });
    ifaddrs *ifap = nullptr;
    if (getifaddrs(&ifap) != 0) return;

    QFont mf(QStringLiteral("Menlo"), 10);
    mf.setStyleHint(QFont::Monospace);

    for (auto *ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || (ifa->ifa_flags & IFF_LOOPBACK)) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;

        auto *sin = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
        char ip_buf[INET_ADDRSTRLEN] = {0};
        const char *ip = inet_ntop(AF_INET, &sin->sin_addr, ip_buf, sizeof(ip_buf));
        if (!ip) continue;

        char mask_buf[INET_ADDRSTRLEN] = {0};
        const char *mask = "";
        if (ifa->ifa_netmask && ifa->ifa_netmask->sa_family == AF_INET) {
            auto *sm = reinterpret_cast<sockaddr_in *>(ifa->ifa_netmask);
            mask = inet_ntop(AF_INET, &sm->sin_addr, mask_buf, sizeof(mask_buf));
            if (!mask) mask = "";
        }

        bool up = (ifa->ifa_flags & IFF_UP);
        bool running = (ifa->ifa_flags & IFF_RUNNING);

        const char *iftype = "";
        if (ifa->ifa_flags & IFF_LOOPBACK) iftype = "Loopback";
        else if (strstr(ifa->ifa_name, "en") == ifa->ifa_name) iftype = "Ethernet";
        else if (strstr(ifa->ifa_name, "wl") == ifa->ifa_name) iftype = "WiFi";
        else if (strstr(ifa->ifa_name, "utun") == ifa->ifa_name) iftype = "Tunnel";
        else if (strstr(ifa->ifa_name, "bridge") == ifa->ifa_name) iftype = "Bridge";
        else if (strstr(ifa->ifa_name, "vlan") == ifa->ifa_name) iftype = "VLAN";
        else if (strstr(ifa->ifa_name, "pktap") == ifa->ifa_name) iftype = "Monitor";

        int r = t->rowCount();
        t->insertRow(r);

        auto *status = new QTableWidgetItem(up && running ? QStringLiteral("✦") : QStringLiteral("・"));
        status->setForeground(up && running ? QColor(Theme::MintDeep) : QColor(Theme::Grey));
        status->setTextAlignment(Qt::AlignCenter);

        auto *name = new QTableWidgetItem(ifa->ifa_name);
        name->setForeground(QColor(Theme::Pink));

        auto *ip_item = new QTableWidgetItem(ip);
        ip_item->setFont(mf);
        ip_item->setForeground(QColor(Theme::BabyBlue));

        auto *mask_item = new QTableWidgetItem(*mask ? mask : "-");
        mask_item->setFont(mf);
        mask_item->setForeground(QColor(Theme::LavenderLight));

        auto *type_item = new QTableWidgetItem(*iftype ? iftype : "-");
        type_item->setForeground(QColor(Theme::Lilac));

        t->setItem(r, 0, status);
        t->setItem(r, 1, name);
        t->setItem(r, 2, ip_item);
        t->setItem(r, 3, mask_item);
        t->setItem(r, 4, type_item);
    }
    freeifaddrs(ifap);
    t->resizeColumnToContents(0);
    t->setColumnWidth(0, 24);
    t->horizontalHeader()->setStretchLastSection(true);
    // card title updated by layout
}

void monitor::refresh_arp_table() {
    auto *t = ui->arp_table;
    t->setRowCount(0);
    t->setHorizontalHeaderLabels({QStringLiteral("IP 地址"), QStringLiteral("MAC 地址")});
    QFont mf(QStringLiteral("Menlo"), 10);
    mf.setStyleHint(QFont::Monospace);

#if defined(__APPLE__)
    std::set<std::string> local;
    ifaddrs *ifap = nullptr;
    if (getifaddrs(&ifap) == 0) {
        for (auto *ifa = ifap; ifa; ifa = ifa->ifa_next) {
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
    for (char *p = buf.data(); p < buf.data() + n;) {
        auto *rtm = reinterpret_cast<rt_msghdr *>(p);
        if (rtm->rtm_version != RTM_VERSION) break;
        if (!(rtm->rtm_flags & RTF_LLINFO) || (rtm->rtm_flags & (RTF_LOCAL | RTF_BROADCAST | RTF_MULTICAST))) {
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
                    snprintf(mb, sizeof(mb), "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
                    hm = true;
                }
            }
            sa = reinterpret_cast<sockaddr *>(reinterpret_cast<char *>(sa) + sl);
        }
        if (!ips || !hm) { p += rtm->rtm_msglen; continue; }
        if (mb[0] == 'f' && mb[1] == 'f') { p += rtm->rtm_msglen; continue; }
        if ((mb[0] == '0' && mb[1] == '0') || (mb[1] & 1)) { p += rtm->rtm_msglen; continue; }
        if (local.count(ips)) { p += rtm->rtm_msglen; continue; }
        if (!seen.insert(std::string(ips) + "@" + mb).second) { p += rtm->rtm_msglen; continue; }
        int row = t->rowCount();
        t->insertRow(row);
        auto *ipx = new QTableWidgetItem(ips);
        ipx->setFont(mf);
        ipx->setForeground(QColor(Theme::MintDeep));
        auto *mx = new QTableWidgetItem(mb);
        mx->setFont(mf);
        mx->setForeground(QColor(Theme::DkMuted));
        t->setItem(row, 0, ipx);
        t->setItem(row, 1, mx);
        p += rtm->rtm_msglen;
    }
#else
    std::ifstream arp("/proc/net/arp");
    if (!arp.is_open()) return;
    std::string line;
    std::getline(arp, line);
    while (std::getline(arp, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string ip, hw_type, flags, mac, mask, dev;
        if (!(iss >> ip >> hw_type >> flags >> mac >> mask >> dev)) continue;
        if (flags.find('L') != std::string::npos) continue;
        if (mac == "00:00:00:00:00:00") continue;
        int row = t->rowCount();
        t->insertRow(row);
        auto *ipx = new QTableWidgetItem(QString::fromStdString(ip));
        ipx->setFont(mf);
        ipx->setForeground(QColor(Theme::MintDeep));
        auto *mx = new QTableWidgetItem(QString::fromStdString(mac));
        mx->setFont(mf);
        mx->setForeground(QColor(Theme::DkMuted));
        t->setItem(row, 0, ipx);
        t->setItem(row, 1, mx);
    }
#endif
    t->resizeColumnToContents(0);
    t->horizontalHeader()->setStretchLastSection(true);
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

void monitor::refresh_network_info() {
    refresh_local_ips();
    refresh_arp_table();
    refresh_quarantine_list();
}

void monitor::refresh_protocol_stats() {
    if (!protocol_table_) return;
    auto s = Nezha::Core::ProtocolStats::instance().snapshot();

    struct Row { QString proto; uint64_t pkts; uint64_t bytes; };
    Row rows[] = {
        {QStringLiteral("TCP"),  s.tcp_pkts,  s.tcp_bytes},
        {QStringLiteral("UDP"),  s.udp_pkts,  s.udp_bytes},
        {QStringLiteral("ICMP"), s.icmp_pkts, s.icmp_bytes},
        {QStringLiteral("HTTP"), s.http_pkts, s.http_bytes},
        {QStringLiteral("Other"),s.other_pkts,s.other_bytes},
    };

    protocol_table_->setRowCount(5);
    for (int i = 0; i < 5; ++i) {
        auto *proto = new QTableWidgetItem(rows[i].proto);
        auto *pkts  = new QTableWidgetItem(QString::number(rows[i].pkts));
        auto *bytes = new QTableWidgetItem(QString::number(rows[i].bytes));

        double pct = s.total_packets > 0
            ? (rows[i].pkts * 100.0 / s.total_packets) : 0.0;
        auto *share = new QTableWidgetItem(
            QStringLiteral("%1%").arg(pct, 0, 'f', 1));

        proto->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        pkts->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        bytes->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        share->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        protocol_table_->setItem(i, 0, proto);
        protocol_table_->setItem(i, 1, pkts);
        protocol_table_->setItem(i, 2, bytes);
        protocol_table_->setItem(i, 3, share);
    }
}

void monitor::refresh_quarantine_list() {
    auto list = Nezha::Database::DatabaseHelper::GetQuarantineList();
    ui->card_threats_value->setText(QString::number(static_cast<int>(list.size())));
    auto *t = ui->quarantine_table;
    t->setRowCount(0);
    t->setHorizontalHeaderLabels({QStringLiteral("IP 地址"), QStringLiteral("隔离原因"), QStringLiteral("威胁评分")});
    QFont mf(QStringLiteral("Menlo"), 10);
    mf.setStyleHint(QFont::Monospace);
    for (const auto &r: list) {
        int row = t->rowCount();
        t->insertRow(row);
        auto *i1 = new QTableWidgetItem(QString::fromStdString(r.ip_address));
        i1->setFont(mf);
        i1->setForeground(QColor(Theme::PinkHot));
        auto *i2 = new QTableWidgetItem(QString::fromStdString(r.reason));
        i2->setForeground(QColor(Theme::PinkLight));
        auto *i3 = new QTableWidgetItem(QString::number(r.threat_score, 'f', 0));
        i3->setForeground(QColor(Theme::DkMuted));
        t->setItem(row, 0, i1);
        t->setItem(row, 1, i2);
        t->setItem(row, 2, i3);
    }
    t->setSortingEnabled(true);
    t->sortByColumn(2, Qt::DescendingOrder);
    t->resizeColumnToContents(0);
    t->horizontalHeader()->setStretchLastSection(true);
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
    for (auto *p: {log_detail_panel_, alert_detail_panel_, honey_detail_panel_}) if (p) p->clear();
}

void monitor::update_sparkline() {
    int cur = log_model_ ? log_model_->total() : 0;
    static int last = 0;
    int delta = std::max(0, cur - last);
    last = cur;
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
    set_qs(ui->qs_types, QStringLiteral("攻击类型"), active_types_.size(), Theme::CyanLight);
}

static QString read_or_default(const QString &path, const QString &fallback) {
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        in.setEncoding(QStringConverter::Utf8);
        QString content = in.readAll();
        if (!content.trimmed().isEmpty()) return content;
    }
    return fallback;
}

static bool write_file_utf8(const QString &path, const QString &content) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) return false;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    return true;
}

// 各平台默认配置模板
#define MONITOR_TEMPLATE \
"# ============================================================\n"\
"# NezhaGuard — 应用监控配置\n"\
"# ============================================================\n"\
"# 格式: 名称 端口 [日志路径...]\n"\
"#   名称:     应用名称（用于日志显示）\n"\
"#   端口:     监听端口（用于检测应用是否在线）\n"\
"#   日志路径: 可选，应用日志文件路径\n"\
"#\n"\
"# 仅端口监控（无日志文件，如 Express.js / Gin 等 stdout 输出）:\n"\
"#   Express 5000\n"\
"#   Gin 7000\n"\
"#\n"\
"# 端口 + 日志文件监控:\n"\
"#   Spring-8000 8000 /var/log/spring8000/app.log\n"\
"# ============================================================\n"

#define CONF_HEADER(label, url_help, url_fmt) \
"# ============================================================\n"\
"# NezhaGuard — " label " 通知配置\n"\
"# ============================================================\n"\
"#\n"\
"# webhook:    " url_help "\n"\
"#             " url_fmt "\n"\
"#\n"\
"# enabled:    是否启用 (true 或 false)\n"\
"#\n"\
"# keywords:   触发关键词（逗号分隔，大小写不敏感）\n"\
"#             当告警内容包含任一关键词时触发通知\n"\
"#\n"\
"# min_severity: 最低告警级别 — trace, debug, info, warn, error, critical\n"\
"#               只有 >= 此级别的告警才会触发\n"\
"# ============================================================\n"

void monitor::load_settings_configs() {
    ui->monitor_conf_edit->setPlainText(
        read_or_default(QStringLiteral("config/monitor_apps.conf"),
                        QStringLiteral(MONITOR_TEMPLATE)));

    ui->database_conf_edit->setPlainText(
        read_or_default(QStringLiteral("config/database.conf"),
            QStringLiteral(
                "# ============================================================\n"
                "# NezhaGuard — 数据库配置\n"
                "# ============================================================\n"
                "# type:     数据库类型 — sqlite, mysql, postgres, oracle, db2\n"
                "# host:     数据库主机地址\n"
                "# port:     数据库端口 (mysql=3306, postgres=5432)\n"
                "# name:     数据库名称\n"
                "# user:     数据库用户名\n"
                "# password: 数据库密码\n"
                "#\n"
                "# sqlite 仅需 name (文件路径)，其他字段忽略\n"
                "# ============================================================\n"
                "type=sqlite\n"
                "host=localhost\n"
                "port=0\n"
                "name=data/nezha_quarantine.db\n"
                "user=\n"
                "password=\n")));

    ui->slack_conf_edit->setPlainText(
        read_or_default(QStringLiteral("config/notifier/slack.conf"),
            QStringLiteral(CONF_HEADER("Slack",
                "Slack Incoming Webhook URL",
                "获取: https://api.slack.com/messaging/webhooks")
                "webhook=https://hooks.slack.com/services/TXXXXX/BXXXXX/xxxxxxxxxxxx\n"
                "enabled=false\n"
                "keywords=sql,注入,暴力,端口扫描,xss,木马,log4j,critical,致命\n"
                "min_severity=warn\n")));

    ui->discord_conf_edit->setPlainText(
        read_or_default(QStringLiteral("config/notifier/discord.conf"),
            QStringLiteral(CONF_HEADER("Discord",
                "Discord Webhook URL",
                "获取: 服务器设置 → 整合 → Webhooks")
                "webhook=https://discord.com/api/webhooks/YOUR/CHANNEL_TOKEN\n"
                "enabled=false\n"
                "keywords=sql,注入,暴力,端口扫描,xss,webshell,木马,critical\n"
                "min_severity=warn\n")));

    ui->dingtalk_conf_edit->setPlainText(
        read_or_default(QStringLiteral("config/notifier/dingtalk.conf"),
            QStringLiteral(CONF_HEADER("钉钉",
                "钉钉群机器人 Webhook URL",
                "获取: 群设置 → 智能群助手 → 添加机器人")
                "webhook=https://oapi.dingtalk.com/robot/send?access_token=YOUR_TOKEN\n"
                "enabled=false\n"
                "keywords=sql,注入,暴力,端口扫描,webshell,木马,critical\n"
                "min_severity=warn\n")));

    ui->feishu_conf_edit->setPlainText(
        read_or_default(QStringLiteral("config/notifier/feishu.conf"),
            QStringLiteral(CONF_HEADER("飞书",
                "飞书群机器人 Webhook URL",
                "获取: 群设置 → 群机器人 → 添加自定义机器人")
                "webhook=https://open.feishu.cn/open-apis/bot/v2/hook/YOUR_HOOK_ID\n"
                "enabled=false\n"
                "keywords=sql,注入,暴力,端口扫描,webshell,木马,critical\n"
                "min_severity=warn\n")));

    ui->wechat_conf_edit->setPlainText(
        read_or_default(QStringLiteral("config/notifier/wechat.conf"),
            QStringLiteral(CONF_HEADER("企业微信",
                "企业微信群机器人 Webhook URL",
                "获取: 群设置 → 群机器人 → 添加机器人")
                "webhook=https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=YOUR_KEY\n"
                "enabled=false\n"
                "keywords=sql,注入,暴力,端口扫描,critical\n"
                "min_severity=warn\n")));

    ui->email_conf_edit->setPlainText(
        read_or_default(QStringLiteral("config/notifier/email.conf"),
            QStringLiteral(CONF_HEADER("邮件",
                "接收告警的邮箱地址",
                "格式: admin@example.com (需系统 mail 命令)")
                "webhook=admin@example.com\n"
                "enabled=false\n"
                "keywords=critical,致命,隔离,quarantine,阻断\n"
                "min_severity=critical\n")));

    ui->telegram_conf_edit->setPlainText(
        read_or_default(QStringLiteral("config/notifier/telegram.conf"),
            QStringLiteral(CONF_HEADER("Telegram",
                "Bot Token (从 @BotFather 获取)",
                "格式: 123456:ABC-DEF1234ghijkl")
                "# chat_id:   目标 Chat/Channel ID\n"
                "#             获取: 将 @getidsbot 拉入群后发送 /id\n"
                "webhook=123456:ABC-DEF1234ghijkl\n"
                "chat_id=-1001234567890\n"
                "enabled=false\n"
                "keywords=sql,注入,暴力,端口扫描,xss,webshell,木马,critical,致命\n"
                "min_severity=warn\n")));

    ui->local_conf_edit->setPlainText(
        read_or_default(QStringLiteral("config/notifier/local.conf"),
            QStringLiteral(
                "# ============================================================\n"
                "# NezhaGuard — 本地桌面通知 配置\n"
                "# ============================================================\n"
                "# 无需 webhook — macOS 系统通知 / Linux notify-send\n"
                "#\n"
                "# enabled:    是否启用 (true 或 false)\n"
                "# keywords:   触发关键词（逗号分隔，大小写不敏感）\n"
                "# min_severity: 最低告警级别 — trace, debug, info, warn, error, critical\n"
                "# ============================================================\n"
                "enabled=true\n"
                "keywords=sql,注入,暴力,SSH,爆破,端口扫描,扫描,xss,webshell,木马,log4j,critical,致命,隔离,阻断\n"
                "min_severity=warn\n")));
}

void monitor::save_monitor_conf() {
    if (write_file_utf8(QStringLiteral("config/monitor_apps.conf"),
                         ui->monitor_conf_edit->toPlainText())) {
        ui->status_label->setText(QStringLiteral("应用监控配置已保存"));
    } else {
        ui->status_label->setText(QStringLiteral("保存失败"));
    }
}

void monitor::save_notifier_conf() {
    auto *tw = ui->settings_tabs;
    auto *w = tw->currentWidget();
    if (w == ui->tab_monitor) {
        save_monitor_conf();
        return;
    }
    QPlainTextEdit *edits[] = {
        nullptr,
        ui->database_conf_edit,
        ui->slack_conf_edit,
        ui->discord_conf_edit,
        ui->dingtalk_conf_edit,
        ui->feishu_conf_edit,
        ui->wechat_conf_edit,
        ui->email_conf_edit,
        ui->telegram_conf_edit,
        ui->local_conf_edit
    };
    const char *paths[] = {
        nullptr,
        "config/database.conf",
        "config/notifier/slack.conf",
        "config/notifier/discord.conf",
        "config/notifier/dingtalk.conf",
        "config/notifier/feishu.conf",
        "config/notifier/wechat.conf",
        "config/notifier/email.conf",
        "config/notifier/telegram.conf",
        "config/notifier/local.conf"
    };
    int idx = tw->currentIndex();
    if (idx >= 0 && idx < 10 && edits[idx] && paths[idx]) {
        if (write_file_utf8(QString::fromUtf8(paths[idx]), edits[idx]->toPlainText()))
            ui->status_label->setText(QString::fromUtf8(paths[idx]) + QStringLiteral(" — 已保存"));
        else
            ui->status_label->setText(QStringLiteral("保存失败"));
    }
}

void monitor::save_database_conf() {
    if (write_file_utf8(QStringLiteral("config/database.conf"),
                         ui->database_conf_edit->toPlainText()))
        ui->status_label->setText(QStringLiteral("数据库配置已保存"));
    else
        ui->status_label->setText(QStringLiteral("保存失败"));
}

void monitor::setup_tray() {
    QIcon app_icon(QStringLiteral(":/app_icon.svg"));
    if (app_icon.isNull())
        app_icon = qApp->style()->standardIcon(QStyle::SP_ComputerIcon);
    qApp->setWindowIcon(app_icon);

    tray_ = new QSystemTrayIcon(this);
    tray_->setIcon(app_icon);
    tray_->setToolTip(QStringLiteral("哪吒网络安全 SIEM"));

    tray_menu_ = new QMenu(this);
    tray_menu_->addAction(QStringLiteral("显示面板"), this, &monitor::tray_show);
    tray_menu_->addSeparator();
    tray_menu_->addAction(QStringLiteral("退出"), this, &monitor::tray_quit);
    tray_->setContextMenu(tray_menu_);

    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::DoubleClick) tray_show();
    });

    tray_->show();
}

void monitor::setup_file_menu() {
    file_menu_ = menuBar()->addMenu(QStringLiteral("文件"));

    export_act_ = file_menu_->addAction(QStringLiteral("导出配置 (.nzc)"));
    export_act_->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
    connect(export_act_, &QAction::triggered, this, &monitor::export_nzc);

    import_act_ = file_menu_->addAction(QStringLiteral("导入配置 (.nzc)"));
    import_act_->setShortcut(QKeySequence(QStringLiteral("Ctrl+I")));
    connect(import_act_, &QAction::triggered, this, &monitor::import_nzc);
}

void monitor::tray_show() {
    show();
    raise();
    activateWindow();
}

void monitor::tray_quit() {
    tray_->hide();
    QApplication::quit();
}

void monitor::export_nzc() {
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出 NezhaGuard 完整状态"),
        QStringLiteral("NezhaGuard_Snapshot.nzc"),
        QStringLiteral("NezhaGuard 状态文件 (*.nzc)"));
    if (path.isEmpty()) return;

    auto read_conf = [](const QString &p) {
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
        QTextStream in(&f);
        in.setEncoding(QStringConverter::Utf8);
        return in.readAll();
    };

    auto model_to_json = [](LogModel *m) {
        QJsonArray arr;
        if (!m) return arr;
        for (int i = 0; i < m->rowCount(); ++i) {
            QModelIndex idx = m->index(i);
            QJsonObject e;
            e[QStringLiteral("ts")] = m->data(idx, LogModel::TimestampRole).toString();
            e[QStringLiteral("level")] = m->data(idx, LogModel::LevelRole).toString();
            e[QStringLiteral("msg")] = m->data(idx, LogModel::MessageRole).toString();
            arr.append(e);
        }
        return arr;
    };

    int uptime = start_time_.secsTo(QTime::currentTime());

    QJsonObject meta;
    meta[QStringLiteral("format")] = QStringLiteral("nzc");
    meta[QStringLiteral("version")] = QStringLiteral("1.0");
    meta[QStringLiteral("author")] = QString::fromUtf8("钟智强");
    meta[QStringLiteral("contact")] = QStringLiteral("johnmelodymel@qq.com");
    meta[QStringLiteral("created_at")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    meta[QStringLiteral("uptime_secs")] = uptime;
    meta[QStringLiteral("log_count")] = log_model_ ? log_model_->total() : 0;
    meta[QStringLiteral("alert_count")] = alert_model_ ? alert_model_->total() : 0;
    meta[QStringLiteral("honey_count")] = honey_model_ ? honey_model_->total() : 0;

    QJsonObject config;
    config[QStringLiteral("monitor_apps")] = read_conf(QStringLiteral("config/monitor_apps.conf"));
    config[QStringLiteral("database")] = read_conf(QStringLiteral("config/database.conf"));
    config[QStringLiteral("notifier_slack")] = read_conf(QStringLiteral("config/notifier/slack.conf"));
    config[QStringLiteral("notifier_discord")] = read_conf(QStringLiteral("config/notifier/discord.conf"));
    config[QStringLiteral("notifier_dingtalk")] = read_conf(QStringLiteral("config/notifier/dingtalk.conf"));
    config[QStringLiteral("notifier_feishu")] = read_conf(QStringLiteral("config/notifier/feishu.conf"));
    config[QStringLiteral("notifier_wechat")] = read_conf(QStringLiteral("config/notifier/wechat.conf"));
    config[QStringLiteral("notifier_email")] = read_conf(QStringLiteral("config/notifier/email.conf"));
    config[QStringLiteral("notifier_telegram")] = read_conf(QStringLiteral("config/notifier/telegram.conf"));
    config[QStringLiteral("notifier_local")] = read_conf(QStringLiteral("config/notifier/local.conf"));

    QJsonObject state;
    state[QStringLiteral("logs")] = model_to_json(log_model_);
    state[QStringLiteral("alerts")] = model_to_json(alert_model_);
    state[QStringLiteral("honeypots")] = model_to_json(honey_model_);

    QJsonArray attackers_arr;
    for (auto it = attackers_.cbegin(); it != attackers_.cend(); ++it) {
        QJsonObject a;
        a[QStringLiteral("ip")] = it.key();
        a[QStringLiteral("score")] = it.value().score;
        a[QStringLiteral("count")] = it.value().count;
        a[QStringLiteral("type")] = it.value().type;
        attackers_arr.append(a);
    }
    state[QStringLiteral("attackers")] = attackers_arr;

    QJsonArray quarantine_arr;
    for (const auto &r: Nezha::Database::DatabaseHelper::GetQuarantineList()) {
        QJsonObject q;
        q[QStringLiteral("ip")] = QString::fromStdString(r.ip_address);
        q[QStringLiteral("reason")] = QString::fromStdString(r.reason);
        q[QStringLiteral("score")] = r.threat_score;
        q[QStringLiteral("date")] = QString::fromStdString(r.quarantined_at);
        quarantine_arr.append(q);
    }
    state[QStringLiteral("quarantine")] = quarantine_arr;

    QJsonObject stats;
    stats[QStringLiteral("sev_crit")] = sev_crit_;
    stats[QStringLiteral("sev_error")] = sev_error_;
    stats[QStringLiteral("sev_warn")] = sev_warn_;
    stats[QStringLiteral("sev_info")] = sev_info_;
    QJsonArray types_arr;
    for (const auto &t: active_types_)
        types_arr.append(t);
    stats[QStringLiteral("active_types")] = types_arr;
    stats[QStringLiteral("sparkline")] = [&]() {
        QJsonArray a;
        for (int v: sparkline_data_) a.append(v);
        return a;
    }();
    state[QStringLiteral("stats")] = stats;

    QJsonObject root;
    root[QStringLiteral("meta")] = meta;
    root[QStringLiteral("config")] = config;
    root[QStringLiteral("state")] = state;

    QJsonDocument doc(root);
    QFile out(path);
    if (out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        out.write(doc.toJson(QJsonDocument::Indented));
        ui->status_label->setText(QStringLiteral("状态已导出至 ") + path);
    } else {
        QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件:\n") + path);
    }
}

void monitor::import_nzc() {
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入 NezhaGuard 状态"),
        QString(), QStringLiteral("NezhaGuard 状态文件 (*.nzc)"));
    if (path.isEmpty()) return;

    QFile in(path);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), QStringLiteral("无法读取文件"));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(in.readAll());
    in.close();

    if (!doc.isObject()) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), QStringLiteral("文件格式无效"));
        return;
    }

    QJsonObject root = doc.object();
    QJsonObject meta = root[QStringLiteral("meta")].toObject();

    if (meta[QStringLiteral("format")].toString() != QStringLiteral("nzc")) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), QStringLiteral("不是有效的 .nzc 文件"));
        return;
    }

    auto write_conf = [](const QString &p, const QString &content) {
        if (content.isEmpty()) return;
        QFile f(p);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream out(&f);
            out.setEncoding(QStringConverter::Utf8);
            out << content;
        }
    };

    QJsonObject config = root[QStringLiteral("config")].toObject();
    write_conf(QStringLiteral("config/monitor_apps.conf"), config[QStringLiteral("monitor_apps")].toString());
    write_conf(QStringLiteral("config/database.conf"), config[QStringLiteral("database")].toString());
    write_conf(QStringLiteral("config/notifier/slack.conf"), config[QStringLiteral("notifier_slack")].toString());
    write_conf(QStringLiteral("config/notifier/discord.conf"), config[QStringLiteral("notifier_discord")].toString());
    write_conf(QStringLiteral("config/notifier/dingtalk.conf"), config[QStringLiteral("notifier_dingtalk")].toString());
    write_conf(QStringLiteral("config/notifier/feishu.conf"), config[QStringLiteral("notifier_feishu")].toString());
    write_conf(QStringLiteral("config/notifier/wechat.conf"), config[QStringLiteral("notifier_wechat")].toString());
    write_conf(QStringLiteral("config/notifier/email.conf"), config[QStringLiteral("notifier_email")].toString());
    write_conf(QStringLiteral("config/notifier/telegram.conf"), config[QStringLiteral("notifier_telegram")].toString());
    write_conf(QStringLiteral("config/notifier/local.conf"), config[QStringLiteral("notifier_local")].toString());

    load_settings_configs();

    QJsonObject state = root[QStringLiteral("state")].toObject();

    auto restore_model = [](LogModel *m, const QJsonArray &arr) {
        if (!m) return;
        m->clear();
        for (const auto &v: arr) {
            QJsonObject e = v.toObject();
            m->append(
                e[QStringLiteral("ts")].toString(),
                e[QStringLiteral("level")].toString(),
                e[QStringLiteral("msg")].toString());
        }
    };

    restore_model(log_model_, state[QStringLiteral("logs")].toArray());
    restore_model(alert_model_, state[QStringLiteral("alerts")].toArray());
    restore_model(honey_model_, state[QStringLiteral("honeypots")].toArray());

    attackers_.clear();
    for (const auto &v: state[QStringLiteral("attackers")].toArray()) {
        QJsonObject a = v.toObject();
        Attacker at;
        at.score = a[QStringLiteral("score")].toDouble();
        at.count = a[QStringLiteral("count")].toInt();
        at.type = a[QStringLiteral("type")].toString();
        attackers_[a[QStringLiteral("ip")].toString()] = at;
    }

    QJsonObject stats = state[QStringLiteral("stats")].toObject();
    sev_crit_ = stats[QStringLiteral("sev_crit")].toInt();
    sev_error_ = stats[QStringLiteral("sev_error")].toInt();
    sev_warn_ = stats[QStringLiteral("sev_warn")].toInt();
    sev_info_ = stats[QStringLiteral("sev_info")].toInt();
    active_types_.clear();
    for (const auto &v: stats[QStringLiteral("active_types")].toArray())
        active_types_.insert(v.toString());
    sparkline_data_.clear();
    for (const auto &v: stats[QStringLiteral("sparkline")].toArray())
        sparkline_data_.append(v.toInt());

    refresh_attackers();
    refresh_quickstats();

    int log_n = state[QStringLiteral("logs")].toArray().size();
    int alert_n = state[QStringLiteral("alerts")].toArray().size();
    int q_n = state[QStringLiteral("quarantine")].toArray().size();

    ui->status_label->setText(QStringLiteral("状态已从 ") + path + QStringLiteral(" 导入"));

    QString author = meta[QStringLiteral("author")].toString();
    QString contact = meta[QStringLiteral("contact")].toString();
    int uptime = meta[QStringLiteral("uptime_secs")].toInt();
    QMessageBox::information(this, QStringLiteral("导入成功"),
        QStringLiteral("状态已恢复\n\n"
                       "作者: %1\n联系: %2\n"
                       "快照时间: %3\n运行时长: %4 秒\n"
                       "日志: %5 条 | 告警: %6 条 | 隔离: %7 条")
            .arg(author, contact,
                 meta[QStringLiteral("created_at")].toString(),
                 QString::number(uptime),
                 QString::number(log_n),
                 QString::number(alert_n),
                 QString::number(q_n)));
}

void monitor::run_nmap_scan(const QString &ip) {
    bool d = dark_mode_;
    auto Bg     = d ? Theme::DkBg        : Theme::LtBg;
    auto Card   = d ? Theme::DkCard      : Theme::LtCard;
    auto Br     = d ? Theme::DkBorder    : Theme::LtBorder;
    auto Tx     = d ? Theme::DkText      : Theme::LtText;
    auto Mu     = d ? Theme::DkMuted     : Theme::LtMuted;
    auto Pk     = d ? Theme::Sakura      : Theme::SakuraDeep;
    auto Sea    = d ? Theme::Seafoam     : Theme::SeafoamDeep;
    auto Wis    = d ? Theme::Wisteria    : Theme::WisteriaDeep;
    auto Sel    = d ? Theme::DkSelected  : Theme::LtSelected;

    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(QStringLiteral("Nmap 扫描"));
    dlg->resize(780, 620);
    dlg->setMinimumSize(600, 440);
    dlg->setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    dlg->setStyleSheet(QStringLiteral("QDialog { background:") + Bg + QStringLiteral("; }"));

    auto *root = new QVBoxLayout(dlg);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(14);

    // ── header card ──
    auto *hdrCard = new QFrame(dlg);
    hdrCard->setObjectName(QStringLiteral("netcard"));
    hdrCard->setStyleSheet(QStringLiteral(
        "QFrame#netcard { background:") + Card + QStringLiteral("; border:1px solid ") + Br +
        QStringLiteral("; border-radius:18px; }"));
    auto *hdrLay = new QHBoxLayout(hdrCard);
    hdrLay->setContentsMargins(20, 16, 20, 16);
    hdrLay->setSpacing(16);

    // target icon + info
    auto *iconLbl = new QLabel(QStringLiteral("目标"));
    iconLbl->setStyleSheet(QStringLiteral(
        "font-size:9px; font-weight:800; color:") + Pk + QStringLiteral("; "
        "background:rgba(242,160,182,0.15); border-radius:10px; padding:4px 10px; "
        "letter-spacing:1px;"));
    hdrLay->addWidget(iconLbl);

    auto *ipLbl = new QLabel(ip);
    ipLbl->setStyleSheet(QStringLiteral(
        "font-size:16px; font-weight:700; color:") + Tx + QStringLiteral("; "
        "font-family:\"SF Mono\",\"Menlo\",monospace; background:transparent;"));
    ipLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hdrLay->addWidget(ipLbl);

    hdrLay->addStretch();

    // status dot
    auto *statusDot = new QLabel(QStringLiteral("扫描中"));
    statusDot->setObjectName(QStringLiteral("scan_status"));
    statusDot->setStyleSheet(QStringLiteral(
        "font-size:9px; font-weight:700; color:") + Sea + QStringLiteral("; "
        "background:rgba(96,208,176,0.15); border-radius:10px; padding:4px 12px; "
        "letter-spacing:1px;"));
    hdrLay->addWidget(statusDot);

    root->addWidget(hdrCard);

    // ── output area ──
    auto *output = new QTextEdit(dlg);
    output->setReadOnly(true);
    output->setStyleSheet(QStringLiteral(
        "QTextEdit { background:") + Card + QStringLiteral("; color:") + Tx + QStringLiteral("; "
        "border:1px solid ") + Br + QStringLiteral("; border-radius:16px; "
        "font-family:\"SF Mono\",\"Menlo\",\"Cascadia Code\",monospace; font-size:12px; "
        "padding:14px; selection-background-color:") + Sel + QStringLiteral("; "
        "selection-color:") + Tx + QStringLiteral("; line-height:1.6; }"
        "QScrollBar:vertical { background:transparent; width:5px; margin:2px; }"
        "QScrollBar::handle:vertical { background:") + Br + QStringLiteral("; border-radius:3px; }"
        "QScrollBar::handle:vertical:hover { background:") + Pk + QStringLiteral("; }"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }"));
    output->setText(QStringLiteral(
        "扫描引擎 v1.0 · 参数: -sT -sV -v\n"
        "目标: %1\n"
        "启动 nmap...\n").arg(ip));
    root->addWidget(output, 1);

    // ── button bar ──
    auto *btnBar = new QHBoxLayout();
    btnBar->setSpacing(10);

    auto mkbtn = [&](const QString &label, const QString &fg) -> QPushButton* {
        auto *b = new QPushButton(label, dlg);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QStringLiteral(
            "QPushButton { background:") + Card + QStringLiteral("; color:") + fg + QStringLiteral("; "
            "border:1px solid ") + Br + QStringLiteral("; border-radius:14px; "
            "padding:7px 20px; font-size:11px; font-weight:600; }"
            "QPushButton:hover { border-color:") + fg + QStringLiteral("; "
            "background:") + Sel + QStringLiteral("; }"));
        return b;
    };

    auto *copyBtn = mkbtn(QStringLiteral("复制结果"), Sea);
    btnBar->addWidget(copyBtn);

    auto *exportBtn = mkbtn(QStringLiteral("导出文本"), Wis);
    btnBar->addWidget(exportBtn);

    btnBar->addStretch();

    auto *closeBtn = mkbtn(QStringLiteral("关闭"), Pk);
    btnBar->addWidget(closeBtn);

    root->addLayout(btnBar);

    // ── nmap process ──
    auto *proc = new QProcess(dlg);
    proc->setProgram(QStringLiteral("nmap"));
    proc->setArguments({QStringLiteral("-sT"), QStringLiteral("-sV"), QStringLiteral("-v"), ip});
    proc->setProcessChannelMode(QProcess::MergedChannels);

    QElapsedTimer *scanTimer = new QElapsedTimer();
    scanTimer->start();

    // cleanup
    connect(dlg, &QDialog::finished, dlg, [proc, dlg]() {
        if (proc->state() != QProcess::NotRunning) {
            proc->terminate();
            if (!proc->waitForFinished(2000)) { proc->kill(); proc->waitForFinished(1000); }
        }
        dlg->deleteLater();
    });
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);

    // copy results
    connect(copyBtn, &QPushButton::clicked, dlg, [output]() {
        QApplication::clipboard()->setText(output->toPlainText());
    });

    // export to file
    connect(exportBtn, &QPushButton::clicked, dlg, [output, ip]() {
        QString path = QFileDialog::getSaveFileName(
            nullptr, QStringLiteral("Export Nmap Results"),
            QStringLiteral("nmap_%1_%2.txt")
                .arg(ip, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))),
            QStringLiteral("Text files (*.txt);;All files (*)"));
        if (!path.isEmpty()) {
            QFile f(path);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                f.write(output->toPlainText().toUtf8());
                f.close();
            }
        }
    });

    // stream output
    connect(proc, &QProcess::readyRead, dlg, [proc, output]() {
        output->append(QString::fromUtf8(proc->readAll()));
    });

    // scan finished
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            dlg, [output, ip, scanTimer, statusDot, Sea, Wis](int code, QProcess::ExitStatus) {
        double elapsed = scanTimer->elapsed() / 1000.0;
        output->append(QString());

        if (code == 0) {
            output->append(QStringLiteral("扫描完成，耗时 %1 秒").arg(elapsed, 0, 'f', 1));
        } else {
            output->append(QStringLiteral("扫描异常退出，代码 %1，耗时 %2 秒").arg(code).arg(elapsed, 0, 'f', 1));
        }
        statusDot->setText(code == 0 ? QStringLiteral("已完成") : QStringLiteral("错误"));
        statusDot->setStyleSheet(QStringLiteral(
            "font-size:9px; font-weight:700; color:") + (code == 0 ? Sea : Wis) + QStringLiteral("; "
            "background:rgba(") + (code == 0 ? QStringLiteral("96,208,176") : QStringLiteral("184,160,232")) +
            QStringLiteral(",0.15); border-radius:10px; padding:4px 12px; letter-spacing:1px;"));
        output->moveCursor(QTextCursor::End);
    });

    dlg->show();
    proc->start();
}
