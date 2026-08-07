//
// Created by 钟智强 on 2026/8/7.
//

#ifndef NEZHAGUARD_SPINNER_WIDGET_H
#define NEZHAGUARD_SPINNER_WIDGET_H

#include <QFrame>

class QVariantAnimation;

class SpinnerWidget : public QFrame {
    Q_OBJECT
public:
    explicit SpinnerWidget(int size = 18, QWidget *parent = nullptr);
    ~SpinnerWidget() override;

    void start();
    void stop();
    bool dark = true;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVariantAnimation *anim_ = nullptr;
    double angle_ = 0.0;
    int size_ = 18;
    bool running_ = false;
};

#endif // NEZHAGUARD_SPINNER_WIDGET_H