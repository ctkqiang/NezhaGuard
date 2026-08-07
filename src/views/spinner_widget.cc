//
// Created by 钟智强 on 2026/8/7.
//

#include "spinner_widget.h"
#include "theme.h"

#include <QPainter>
#include <QVariantAnimation>

SpinnerWidget::SpinnerWidget(int size, QWidget *parent)
    : QFrame(parent), size_(size) {
    setFixedSize(size_ + 8, size_ + 8);
    setFrameShape(QFrame::NoFrame);

    anim_ = new QVariantAnimation(this);
    anim_->setDuration(900);
    anim_->setStartValue(0.0);
    anim_->setEndValue(360.0);
    anim_->setLoopCount(-1);
    anim_->setEasingCurve(QEasingCurve::InOutQuad);
    connect(anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        angle_ = v.toDouble();
        update();
    });
}

SpinnerWidget::~SpinnerWidget() {
    if (anim_) anim_->stop();
}

void SpinnerWidget::start() {
    running_ = true;
    show();
    anim_->start();
}

void SpinnerWidget::stop() {
    running_ = false;
    anim_->stop();
    hide();
}

void SpinnerWidget::paintEvent(QPaintEvent *) {
    if (!running_) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int cx = width() / 2, cy = height() / 2;
    int r = size_ / 2;

    auto color = dark ? QColor(Theme::Strawberry) : QColor(Theme::RosyDeep);

    // draw 12-segment arc with trailing fade
    for (int i = 11; i >= 0; --i) {
        double segAngle = angle_ - i * 28.0; // 28° spacing
        double alpha = 0.12 + (1.0 - i / 12.0) * 0.70;
        color.setAlphaF(alpha);
        p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap));

        double rad = qDegreesToRadians(segAngle);
        double innerX = cx + r * 0.35 * std::cos(rad);
        double innerY = cy - r * 0.35 * std::sin(rad);
        double outerX = cx + r * 0.85 * std::cos(rad);
        double outerY = cy - r * 0.85 * std::sin(rad);

        p.drawLine(QPointF(innerX, innerY), QPointF(outerX, outerY));
    }
}