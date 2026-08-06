//
// Created by 钟智强 on 2026/8/2.
//

#include "radar_widget.h"
#include "theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <random>

RadarWidget::RadarWidget(QWidget *parent) : QFrame(parent) {
    // main sweep — ~3s per revolution, smooth linear
    sweep_anim_ = new QVariantAnimation(this);
    sweep_anim_->setDuration(3200);
    sweep_anim_->setStartValue(0.0);
    sweep_anim_->setEndValue(360.0);
    sweep_anim_->setLoopCount(-1);
    sweep_anim_->setEasingCurve(QEasingCurve::Linear);
    connect(sweep_anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        sweep_angle_ = v.toDouble();
        update();
    });
    sweep_anim_->start();

    // pulse / breathing — drives center glow + device auras
    pulse_anim_ = new QVariantAnimation(this);
    pulse_anim_->setDuration(2200);
    pulse_anim_->setStartValue(0.0);
    pulse_anim_->setEndValue(2.0 * M_PI);
    pulse_anim_->setLoopCount(-1);
    pulse_anim_->setEasingCurve(QEasingCurve::InOutSine);
    connect(pulse_anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        pulse_phase_ = v.toDouble();
        update();
    });
    pulse_anim_->start();

    // ambient noise particles — 30 fps
    particle_timer_ = new QTimer(this);
    particle_timer_->setInterval(33);
    connect(particle_timer_, &QTimer::timeout, this, [this]() {
        spawn_particles();
        update();
    });
    particle_timer_->start();

    frame_timer_.start();
}

RadarWidget::~RadarWidget() {
    sweep_anim_->stop();
    pulse_anim_->stop();
    particle_timer_->stop();
}

void RadarWidget::resizeEvent(QResizeEvent *) {
    center_x_ = width() / 2;
    center_y_ = height() / 2;
    radius_  = std::min(center_x_, center_y_) - 28;
}

void RadarWidget::spawn_particles() {
    static std::mt19937 rng(42);
    static std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * M_PI);
    static std::uniform_real_distribution<double> dist_dist(0.1, 0.95);

    // spawn 1-3 new particles per tick
    int n = std::uniform_int_distribution<int>(1, 3)(rng);
    for (int i = 0; i < n; ++i) {
        particles_.push_back({angle_dist(rng), dist_dist(rng), 0.7, 1.0});
    }

    // cap particles & age existing ones
    double dt = frame_timer_.restart() / 1000.0;
    for (auto it = particles_.begin(); it != particles_.end(); ) {
        it->life -= dt * 0.8;
        it->alpha = std::max(0.0, it->life);
        if (it->life <= 0.0)
            it = particles_.erase(it);
        else
            ++it;
    }
    while (particles_.size() > 80) particles_.pop_front();
}

void RadarWidget::set_devices(const QVector<RadarDevice> &devices) {
    devices_ = devices;
    update();
}

void RadarWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (center_x_ == 0) { center_x_ = width()/2; center_y_ = height()/2; radius_ = std::min(center_x_, center_y_) - 28; }

    int cx = center_x_, cy = center_y_, r = radius_;
    if (r <= 10) return;

    auto bg  = dark ? QColor(Theme::DkBg)   : QColor(Theme::LtBg);
    auto fg  = dark ? QColor(Theme::DkText) : QColor(Theme::LtText);
    auto mu  = dark ? QColor(Theme::DkMuted): QColor(Theme::LtMuted);

    // ── background ──
    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 16, 16);

    // ── range rings (3 concentric, labelled) ──
    for (int i = 1; i <= 3; ++i) {
        int rr = r * i / 3;
        QColor ringC = mu;
        ringC.setAlpha(dark ? 45 : 60);
        p.setPen(QPen(ringC, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPoint(cx, cy), rr, rr);

        // subtle inner glow trace
        ringC.setAlpha(dark ? 12 : 20);
        p.setPen(QPen(ringC, 2.5));
        p.drawEllipse(QPoint(cx, cy), rr, rr);
    }

    // ── crosshair (thin) ──
    QColor xc = mu;
    xc.setAlpha(dark ? 40 : 55);
    p.setPen(QPen(xc, 0.7));
    p.drawLine(cx - r, cy, cx + r, cy);
    p.drawLine(cx, cy - r, cx, cy + r);

    // ── ambient particles (radar noise "blips") ──
    for (const auto &pt : particles_) {
        double angle = pt.angle + sweep_angle_ * M_PI / 180.0 * 0.15; // slight drift with sweep
        int px = cx + static_cast<int>(r * pt.dist * std::cos(angle));
        int py = cy - static_cast<int>(r * pt.dist * std::sin(angle));
        QColor pc(dark ? Theme::Seafoam : Theme::SeafoamDeep);
        pc.setAlphaF(pt.alpha * 0.35);
        p.setBrush(pc);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(px, py), 2, 2);
    }

    // ── sweep trail (multi-segment fade) ──
    double sweep_rad = qDegreesToRadians(sweep_angle_);
    int trail_segments = 12;
    for (int t = trail_segments; t >= 0; --t) {
        double ta = sweep_rad - t * 0.045;
        double alpha = 1.0 - static_cast<double>(t) / (trail_segments + 1);
        int tx = cx + static_cast<int>(r * std::cos(ta));
        int ty = cy - static_cast<int>(r * std::sin(ta));

        QColor trailC(dark ? Theme::Seafoam : Theme::SeafoamDeep);
        trailC.setAlphaF(alpha * 0.55);
        p.setPen(QPen(trailC, 1.0 + alpha * 1.5));
        p.drawLine(cx, cy, tx, ty);
    }

    // ── sweep glow arc (sector behind the sweep line) ──
    QConicalGradient cone(QPointF(cx, cy), -sweep_angle_ + 90);
    QColor arcC = dark ? QColor(Theme::Seafoam) : QColor(Theme::SeafoamDeep);
    arcC.setAlpha(22);
    cone.setColorAt(0.00, arcC);
    arcC.setAlpha(10);
    cone.setColorAt(0.06, arcC);
    cone.setColorAt(0.15, Qt::transparent);
    cone.setColorAt(1.00, Qt::transparent);
    p.setPen(Qt::NoPen);
    p.setBrush(cone);
    p.drawPie(QRect(cx - r, cy - r, r * 2, r * 2),
              static_cast<int>((-sweep_angle_ - 28) * 16), 56 * 16);

    // ── main sweep line ──
    int sx = cx + static_cast<int>(r * std::cos(sweep_rad));
    int sy = cy - static_cast<int>(r * std::sin(sweep_rad));

    // glow under the line
    QColor sweepGlow(dark ? Theme::SeafoamLight : Theme::SeafoamDeep);
    sweepGlow.setAlpha(70);
    p.setPen(QPen(sweepGlow, 4.0));
    p.drawLine(cx, cy, sx, sy);

    // sharp line on top
    QColor sweepLine(dark ? Theme::Seafoam : Theme::SeafoamDeep);
    sweepLine.setAlpha(220);
    p.setPen(QPen(sweepLine, 1.5));
    p.drawLine(cx, cy, sx, sy);

    // tip dot
    p.setBrush(QColor(Theme::SeafoamLight));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPoint(sx, sy), 4, 4);
    QColor tipInner(dark ? Theme::Cream : Theme::Cream);
    p.setBrush(tipInner);
    p.drawEllipse(QPoint(sx, sy), 2, 2);

    // ── devices (positioned on rings by IP hash) ──
    QFont df(QStringLiteral("SF Mono"), 8);
    df.setStyleHint(QFont::Monospace);
    for (const auto &d : devices_) {
        // deterministic position from IP hash
        auto parts = d.ip.split('.');
        uint32_t hash = qHash(d.ip) & 0xFFFF;
        double angle  = (hash % 360) * M_PI / 180.0;
        double dist   = 0.22 + (hash >> 8) / 65535.0 * 0.70;

        int dx = cx + static_cast<int>(r * dist * std::cos(angle));
        int dy = cy - static_cast<int>(r * dist * std::sin(angle));

        // breathing aura
        double aura_radius = 6.0 + std::sin(pulse_phase_ + angle) * 3.0;
        QColor auraC = dark ? QColor(Theme::Sakura) : QColor(Theme::SakuraDeep);
        auraC.setAlphaF(0.15 + std::cos(pulse_phase_ + angle) * 0.08);
        p.setBrush(auraC);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(dx, dy), static_cast<int>(aura_radius), static_cast<int>(aura_radius));

        // outer ring
        QColor ringC = dark ? QColor(Theme::SakuraLight) : QColor(Theme::SakuraDeep);
        ringC.setAlpha(140);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(ringC, 1.0));
        p.drawEllipse(QPoint(dx, dy), 5, 5);

        // solid dot
        p.setBrush(dark ? QColor(Theme::Sakura) : QColor(Theme::SakuraDeep));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(dx, dy), 3, 3);

        // label — show last octet only
        p.setPen(fg);
        p.setFont(df);
        QString label = parts.size() == 4 ? QStringLiteral(".%1").arg(parts.last()) : d.ip;
        p.drawText(QRect(dx - 20, dy - 22, 40, 14), Qt::AlignCenter, label);
    }

    // ── center core ──
    double pulse = 4.0 + std::sin(pulse_phase_) * 4.0;
    QColor pulseC = dark ? QColor(Theme::Seafoam) : QColor(Theme::SeafoamDeep);
    pulseC.setAlphaF(0.20);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(pulseC, 1.5));
    p.drawEllipse(QPoint(cx, cy), static_cast<int>(pulse + 6), static_cast<int>(pulse + 6));

    QRadialGradient coreGrad(cx, cy, 8);
    coreGrad.setColorAt(0.0, dark ? QColor(Theme::SeafoamLight) : QColor(Theme::SeafoamDeep));
    coreGrad.setColorAt(0.5, dark ? QColor(Theme::Seafoam) : QColor(Theme::Seafoam));
    coreGrad.setColorAt(1.0, dark ? QColor(Theme::Seafoam).darker(180) : QColor(Theme::SeafoamDeep).darker(150));
    p.setPen(Qt::NoPen);
    p.setBrush(coreGrad);
    p.drawEllipse(QPoint(cx, cy), 5, 5);

    p.setBrush(dark ? QColor(Theme::Cream) : QColor(Theme::Cream));
    p.drawEllipse(QPoint(cx, cy), 2, 2);

    // ── corner info overlay ──
    p.setPen(mu);
    p.setFont(QFont(QStringLiteral("SF Pro Rounded"), 9));
    p.drawText(QRect(10, 6, 200, 16), Qt::AlignLeft,
               QStringLiteral("Devices: %1  ·  %2 RPM").arg(devices_.size()).arg(60000.0 / sweep_anim_->duration(), 0, 'f', 1));
}