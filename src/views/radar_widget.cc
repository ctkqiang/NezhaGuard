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
    sweep_anim_->setDuration(2800);
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
    pulse_anim_->setDuration(2000);
    pulse_anim_->setStartValue(0.0);
    pulse_anim_->setEndValue(2.0 * M_PI);
    pulse_anim_->setLoopCount(-1);
    pulse_anim_->setEasingCurve(QEasingCurve::InOutSine);
    connect(pulse_anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        pulse_phase_ = v.toDouble();
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

    int w = width(), h = height();
    int cx = w / 2, cy = h / 2;
    int r  = std::min(cx, cy) - 20;
    if (r < 20) return;

    auto bg  = dark ? QColor(Theme::DkCard) : QColor(Theme::LtCard);
    auto fg  = dark ? QColor(Theme::DkText) : Theme::DkText;
    auto mu  = dark ? QColor(Theme::DkMuted): QColor(Theme::LtMuted);

    // clean background — no clipping
    p.fillRect(rect(), bg);

    // range rings
    for (int i = 1; i <= 3; ++i) {
        int rr = r * i / 3;
        QColor rc = mu;
        rc.setAlpha(dark ? 40 : 55);
        p.setPen(QPen(rc, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPoint(cx, cy), rr, rr);
    }

    // crosshair
    QColor xc = mu;
    xc.setAlpha(dark ? 35 : 50);
    p.setPen(QPen(xc, 0.6));
    p.drawLine(cx - r, cy, cx + r, cy);
    p.drawLine(cx, cy - r, cx, cy + r);

    // sweep trail (12-segment fade)
    double sweep_rad = qDegreesToRadians(sweep_angle_);
    for (int t = 12; t >= 0; --t) {
        double ta = sweep_rad - t * 0.04;
        double a = 1.0 - t / 13.0;
        int tx = cx + static_cast<int>(r * std::cos(ta));
        int ty = cy - static_cast<int>(r * std::sin(ta));
        QColor tc(dark ? Theme::Mint : Theme::MintDeep);
        tc.setAlphaF(a * 0.5);
        p.setPen(QPen(tc, 1.0 + a * 1.5));
        p.drawLine(cx, cy, tx, ty);
    }

    // sweep glow arc
    QConicalGradient cone(QPointF(cx, cy), -sweep_angle_ + 90);
    QColor arcC = dark ? QColor(Theme::Mint) : QColor(Theme::MintDeep);
    arcC.setAlpha(20);
    cone.setColorAt(0.00, arcC);
    arcC.setAlpha(8);
    cone.setColorAt(0.06, arcC);
    cone.setColorAt(0.14, Qt::transparent);
    cone.setColorAt(1.00, Qt::transparent);
    p.setPen(Qt::NoPen);
    p.setBrush(cone);
    p.drawPie(QRect(cx - r, cy - r, r * 2, r * 2),
              static_cast<int>((-sweep_angle_ - 26) * 16), 52 * 16);

    // main sweep line
    int sx = cx + static_cast<int>(r * std::cos(sweep_rad));
    int sy = cy - static_cast<int>(r * std::sin(sweep_rad));

    QColor sg(dark ? Theme::MintLight : Theme::MintDeep);
    sg.setAlpha(60);
    p.setPen(QPen(sg, 3.5));
    p.drawLine(cx, cy, sx, sy);

    QColor sl(dark ? Theme::Mint : Theme::Mint);
    sl.setAlpha(200);
    p.setPen(QPen(sl, 1.5));
    p.drawLine(cx, cy, sx, sy);

    // tip dot
    p.setBrush(dark ? QColor(Theme::Cream) : Theme::Cream);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPoint(sx, sy), 3, 3);

    // devices
    QFont df(QStringLiteral("SF Mono"), 9);
    df.setStyleHint(QFont::Monospace);
    for (const auto &d : devices_) {
        uint32_t hash = qHash(d.ip) & 0xFFFF;
        double angle  = (hash % 360) * M_PI / 180.0;
        double dist   = 0.18 + (hash / 65535.0) * 0.72;
        int dx = cx + static_cast<int>(r * dist * std::cos(angle));
        int dy = cy - static_cast<int>(r * dist * std::sin(angle));

        auto dotColor = dark ? QColor(Theme::Strawberry) : QColor(Theme::RosyDeep);
        auto glowColor = dark ? QColor(Theme::StrawberryLight) : QColor(Theme::RosyLight);

        // outer glow ripple — sonar ping expanding ring
        double ripplePhase = std::fmod(pulse_phase_ * 1.7 + angle, 2.0 * M_PI);
        double rippleR = 5.0 + ripplePhase * 7.0;
        QColor rp(dark ? Theme::Strawberry : Theme::RosyDeep);
        rp.setAlphaF(std::max(0.0, 0.18 - ripplePhase * 0.06));
        p.setPen(QPen(rp, 0.8));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPoint(dx, dy), static_cast<int>(rippleR), static_cast<int>(rippleR));

        // mid aura — soft breathing glow
        double ar = 7.0 + std::sin(pulse_phase_ + angle) * 3.5;
        for (int layer = 2; layer >= 0; --layer) {
            double lr = ar - layer * 2.5;
            if (lr < 1.0) continue;
            QColor au(glowColor);
            au.setAlphaF(0.06 + layer * 0.04 + std::cos(pulse_phase_ + angle) * 0.03);
            p.setBrush(au);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPoint(dx, dy), static_cast<int>(lr), static_cast<int>(lr));
        }

        // ring — pulsing slightly
        double ringR = 5.0 + std::sin(pulse_phase_ + angle) * 0.8;
        QColor rc(glowColor);
        rc.setAlpha(160);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(rc, 1.2));
        p.drawEllipse(QPoint(dx, dy), static_cast<int>(ringR), static_cast<int>(ringR));

        // dot — radial gradient for depth
        double dotR = 3.5;
        QRadialGradient dg(dx - 0.5, dy - 0.5, dotR);
        dg.setColorAt(0.0, dark ? QColor(Theme::Cream) : QColor(255, 255, 255));
        dg.setColorAt(0.35, dark ? QColor(Theme::StrawberryLight) : QColor(Theme::RosyLight));
        dg.setColorAt(1.0, dotColor);
        p.setPen(Qt::NoPen);
        p.setBrush(dg);
        p.drawEllipse(QPoint(dx, dy), static_cast<int>(dotR), static_cast<int>(dotR));

        // label with subtle badge background
        auto parts = d.ip.split('.');
        QString labelText = parts.size() == 4 ? QStringLiteral(".%1").arg(parts.last()) : d.ip;
        QFontMetrics fm(df);
        int labelW = fm.horizontalAdvance(labelText) + 14;
        QRect labelBg(dx - labelW / 2, dy - 24, labelW, 16);
        QColor lb(dark ? QColor(Theme::DkCard) : QColor(Theme::LtCard));
        lb.setAlpha(180);
        p.setBrush(lb);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(labelBg, 7, 7);
        p.setPen(fg);
        p.setFont(df);
        p.drawText(labelBg, Qt::AlignCenter, labelText);
    }

    // center core
    double pulse = 4.0 + std::sin(pulse_phase_) * 4.0;
    QColor pc(dark ? Theme::Mint : Theme::MintDeep);
    pc.setAlphaF(0.18);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(pc, 1.5));
    p.drawEllipse(QPoint(cx, cy), static_cast<int>(pulse + 6), static_cast<int>(pulse + 6));

    QRadialGradient cg(cx, cy, 7);
    cg.setColorAt(0.0, dark ? QColor(Theme::MintLight) : QColor(Theme::Mint));
    cg.setColorAt(0.5, dark ? QColor(Theme::Mint) : QColor(Theme::MintDeep));
    cg.setColorAt(1.0, dark ? QColor(Theme::Mint).darker(180) : QColor(Theme::MintDeep).darker(150));
    p.setPen(Qt::NoPen);
    p.setBrush(cg);
    p.drawEllipse(QPoint(cx, cy), 5, 5);

    p.setBrush(dark ? QColor(Theme::Cream) : Theme::Cream);
    p.drawEllipse(QPoint(cx, cy), 2, 2);

    // corner label
    p.setPen(mu);
    p.setFont(QFont(QStringLiteral("SF Pro Rounded"), 9));
    p.drawText(QRect(10, 6, 200, 16), Qt::AlignLeft,
               QStringLiteral("设备: %1 · %2 转/分").arg(devices_.size()).arg(60000.0 / 2800.0, 0, 'f', 1));
}