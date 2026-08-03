//
// Created by 钟智强 on 2026/8/2.
//

#include "radar_widget.h"
#include "theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

RadarWidget::RadarWidget(QWidget *parent) : QFrame(parent) {
    sweep_timer_ = new QTimer(this);
    connect(sweep_timer_, &QTimer::timeout, this, [this]() {
        sweep_angle_ = (sweep_angle_ + 2) % 360;
        update();
    });
    sweep_timer_->start(40);
}

RadarWidget::~RadarWidget() { if (sweep_timer_) sweep_timer_->stop(); }

void RadarWidget::set_devices(const QVector<RadarDevice> &devices) {
    devices_ = devices;
    update();
}

void RadarWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    auto bg = dark ? QColor(Theme::DkCard) : QColor(Theme::LtCard);
    auto ring = dark ? QColor(Theme::DkBorder) : QColor(Theme::LtBorder);
    auto fg = dark ? QColor(Theme::DkText) : QColor(Theme::LtText);

    p.setBrush(bg); p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 10, 10);

    int cx = width() / 2, cy = height() / 2;
    int r = std::min(cx, cy) - 20;

    // radar rings
    for (int i = 1; i <= 4; ++i) {
        int rr = r * i / 4;
        p.setPen(QPen(ring, 0.5, Qt::DotLine));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPoint(cx, cy), rr, rr);
    }

    // crosshairs
    p.setPen(QPen(ring, 0.3));
    p.drawLine(cx - r, cy, cx + r, cy);
    p.drawLine(cx, cy - r, cx, cy + r);

    // sweep line ♡
    double rad = qDegreesToRadians(static_cast<double>(sweep_angle_));
    QPen sweepPen(QColor(Theme::PinkLight), 1.5);
    p.setPen(sweepPen);
    p.drawLine(cx, cy, cx + static_cast<int>(r * std::cos(rad)), cy - static_cast<int>(r * std::sin(rad)));

    // sweep arc glow ♡
    QConicalGradient cone(cx, cy, -sweep_angle_);
    cone.setColorAt(0.0, QColor(Theme::PinkDeep).darker(180));
    cone.setColorAt(0.1, QColor(Theme::PinkLight).lighter(120));
    cone.setColorAt(0.2, QColor(Theme::Lavender).darker(180));
    p.setPen(Qt::NoPen);
    p.setBrush(cone);
    p.drawPie(QRect(cx - r, cy - r, r * 2, r * 2), (sweep_angle_ - 30) * 16, 60 * 16);

    // devices
    QFont df(QStringLiteral("Menlo"), 8);
    df.setStyleHint(QFont::Monospace);
    for (const auto &d: devices_) {
        // angle from last IP octet, radius from hash of full IP
        auto parts = d.ip.split('.');
        int octet = parts.size() == 4 ? parts.last().toInt() : 0;
        double a = octet * 360.0 / 256.0;
        double dist = 0.3 + (std::hash<std::string>{}(d.ip.toStdString()) % 70) / 100.0;
        int dx = cx + static_cast<int>(r * dist * std::cos(qDegreesToRadians(a)));
        int dy = cy - static_cast<int>(r * dist * std::sin(qDegreesToRadians(a)));

        // dot ♡
        p.setBrush(QColor(Theme::MintDeep));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(dx, dy), 4, 4);

        // glow
        QColor glow(Theme::MintDeep);
        glow.setAlpha(50);
        p.setBrush(glow);
        p.drawEllipse(QPoint(dx, dy), 8, 8);

        // label
        p.setPen(fg);
        p.setFont(df);
        QString label = parts.size() == 4 ? parts.last() : d.ip;
        p.drawText(QRect(dx - 20, dy - 18, 40, 14), Qt::AlignCenter, label);
    }

    // center dot ♡
    p.setBrush(QColor(Theme::PinkLight));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPoint(cx, cy), 5, 5);

    // title ♡
    p.setPen(QColor(Theme::Pink));
    p.setFont(QFont(QStringLiteral("PingFang SC"), 9));
    p.drawText(QRect(8, 4, 200, 16), Qt::AlignLeft,
               QStringLiteral("♡ 设备雷达 — %1 台").arg(devices_.size()));
}
