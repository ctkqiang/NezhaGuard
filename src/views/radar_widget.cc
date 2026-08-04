//
// Created by 钟智强 on 2026/8/2.
//

#include "radar_widget.h"
#include "theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

RadarWidget::RadarWidget(QWidget *parent) : QFrame(parent) {
    sweep_anim_ = new QVariantAnimation(this);
    sweep_anim_->setDuration(7200);
    sweep_anim_->setStartValue(0.0);
    sweep_anim_->setEndValue(360.0);
    sweep_anim_->setLoopCount(-1);
    sweep_anim_->setEasingCurve(QEasingCurve::Linear);
    connect(sweep_anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        sweep_angle_ = v.toDouble();
        update();
    });
    sweep_anim_->start();

    pulse_anim_ = new QVariantAnimation(this);
    pulse_anim_->setDuration(1800);
    pulse_anim_->setStartValue(0.0);
    pulse_anim_->setEndValue(1.0);
    pulse_anim_->setLoopCount(-1);
    pulse_anim_->setEasingCurve(QEasingCurve::InOutSine);
    connect(pulse_anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        pulse_radius_ = v.toDouble();
        update();
    });
    pulse_anim_->start();
}

RadarWidget::~RadarWidget() {
    if (sweep_anim_) sweep_anim_->stop();
    if (pulse_anim_) pulse_anim_->stop();
}

void RadarWidget::set_devices(const QVector<RadarDevice> &devices) {
    devices_ = devices;
    update();
}

void RadarWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    auto bg   = dark ? QColor(Theme::DkCard)   : QColor(Theme::LtCard);
    auto ring = dark ? QColor(Theme::DkBorder) : QColor(Theme::LtBorder);
    auto fg   = dark ? QColor(Theme::DkText)   : QColor(Theme::LtText);

    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 10, 10);

    int cx = width() / 2, cy = height() / 2;
    int r  = std::min(cx, cy) - 22;

    // -- concentric rings with alternating Cyan/Pink tints --
    for (int i = 1; i <= 4; ++i) {
        int rr = r * i / 4;
        QColor ringCol = (i % 2 == 0)
            ? QColor(Theme::CyanLight).darker(150)
            : QColor(Theme::PinkLight).darker(160);
        ringCol.setAlpha(60);
        p.setPen(QPen(ringCol, 0.8, Qt::DotLine));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPoint(cx, cy), rr, rr);

        // subtle inner glow ring
        QColor innerGlow = (i % 2 == 0)
            ? QColor(Theme::CyanNeon)
            : QColor(Theme::Pink);
        innerGlow.setAlpha(15);
        p.setPen(QPen(innerGlow, 2.0));
        p.drawEllipse(QPoint(cx, cy), rr, rr);
    }

    // -- crosshairs --
    QColor xhair(ring);
    xhair.setAlpha(50);
    p.setPen(QPen(xhair, 0.5));
    p.drawLine(cx - r, cy, cx + r, cy);
    p.drawLine(cx, cy - r, cx, cy + r);

    // diagonal subtle cross
    xhair.setAlpha(25);
    p.setPen(QPen(xhair, 0.3, Qt::DotLine));
    int d45 = static_cast<int>(r * 0.707);
    p.drawLine(cx - d45, cy - d45, cx + d45, cy + d45);
    p.drawLine(cx - d45, cy + d45, cx + d45, cy - d45);

    // -- fading trail behind sweep --
    int trail_len = 5;
    for (int t = trail_len; t >= 0; --t) {
        double trail_angle = sweep_angle_ - t * 4.0;
        double trad = qDegreesToRadians(trail_angle);
        double alpha = 1.0 - static_cast<double>(t) / (trail_len + 1);
        int lx = cx + static_cast<int>(r * std::cos(trad));
        int ly = cy - static_cast<int>(r * std::sin(trad));

        QColor trailCol = QColor(Theme::CyanNeon);
        trailCol.setAlphaF(alpha * 0.6);
        p.setPen(QPen(trailCol, 1.0 + alpha * 1.2));
        p.drawLine(cx, cy, lx, ly);
    }

    // -- main sweep line --
    double rad = qDegreesToRadians(sweep_angle_);
    QPen sweepPen(QColor(Theme::PinkHot), 1.8);
    p.setPen(sweepPen);
    int sx = cx + static_cast<int>(r * std::cos(rad));
    int sy = cy - static_cast<int>(r * std::sin(rad));
    p.drawLine(cx, cy, sx, sy);

    // sweep tip dot
    p.setBrush(QColor(Theme::PinkLight));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPoint(sx, sy), 3, 3);

    // -- sweep arc glow (Cyan -> Pink -> Lavender gradient) --
    QConicalGradient cone(cx, cy, -sweep_angle_);
    cone.setColorAt(0.00, QColor(Theme::CyanNeon).darker(120));
    cone.setColorAt(0.08, QColor(Theme::CyanLight).lighter(110));
    cone.setColorAt(0.14, QColor(Theme::PinkLight).lighter(110));
    cone.setColorAt(0.22, QColor(Theme::Lavender).darker(160));
    cone.setColorAt(0.30, Qt::transparent);
    cone.setColorAt(1.00, Qt::transparent);
    p.setPen(Qt::NoPen);
    p.setBrush(cone);
    p.drawPie(QRect(cx - r, cy - r, r * 2, r * 2),
              static_cast<int>((sweep_angle_ - 40) * 16), 80 * 16);

    // -- devices --
    QFont df(QStringLiteral("Menlo"), 9);
    df.setStyleHint(QFont::Monospace);
    for (const auto &d: devices_) {
        auto parts = d.ip.split('.');
        int octet = parts.size() == 4 ? parts.last().toInt() : 0;
        double a    = octet * 360.0 / 256.0;
        double dist = 0.28 + (static_cast<unsigned>(parts.size() == 4 ? parts[2].toInt() : 0) % 70) / 100.0;
        int dx = cx + static_cast<int>(r * dist * std::cos(qDegreesToRadians(a)));
        int dy = cy - static_cast<int>(r * dist * std::sin(qDegreesToRadians(a)));

        // outer glow — Cyan
        QColor glowC(Theme::CyanNeon);
        glowC.setAlpha(40 + static_cast<int>(pulse_radius_ * 20));
        p.setBrush(glowC);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(dx, dy), 10, 10);

        // dot — Pink
        p.setBrush(QColor(Theme::PinkHot));
        p.drawEllipse(QPoint(dx, dy), 4, 4);

        // inner dot — white
        p.setBrush(QColor(Theme::White));
        p.drawEllipse(QPoint(dx, dy), 2, 2);

        // label
        p.setPen(fg);
        p.setFont(df);
        QString label = parts.size() == 4 ? parts.last() : d.ip;
        p.drawText(QRect(dx - 22, dy - 20, 44, 14), Qt::AlignCenter, label);
    }

    // -- center dot with pulse --
    double pulse = 5.0 + pulse_radius_ * 8.0;
    // outer pulse ring
    QColor pulseC(Theme::PinkBlush);
    pulseC.setAlphaF(0.25 * (1.0 - pulse_radius_));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(pulseC, 2.0));
    p.drawEllipse(QPoint(cx, cy), static_cast<int>(pulse), static_cast<int>(pulse));

    // inner core
    QColor coreC1(Theme::PinkLight);
    QColor coreC2(Theme::PinkHot);
    QRadialGradient coreGrad(cx, cy, 7);
    coreGrad.setColorAt(0.0, coreC1);
    coreGrad.setColorAt(0.5, coreC2);
    coreGrad.setColorAt(1.0, QColor(Theme::PinkDeep).darker(140));
    p.setPen(Qt::NoPen);
    p.setBrush(coreGrad);
    p.drawEllipse(QPoint(cx, cy), 6, 6);

    // inner white spark
    p.setBrush(QColor(Theme::White));
    p.drawEllipse(QPoint(cx, cy), 2, 2);

    // -- title --
    p.setPen(QColor(Theme::Pink));
    p.setFont(QFont(QStringLiteral("PingFang SC"), 9));
    p.drawText(QRect(8, 4, 240, 16), Qt::AlignLeft,
               QStringLiteral("Device Radar — %1").arg(devices_.size()));
}