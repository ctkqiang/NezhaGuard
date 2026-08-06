//
// Created by 钟智强 on 2026/8/2.
//

#ifndef NEZHAGUARD_RADAR_WIDGET_H
#define NEZHAGUARD_RADAR_WIDGET_H

#include <QFrame>
#include <QString>
#include <QVariantAnimation>
#include <QVector>

struct RadarDevice {
    QString ip;
    QString mac;
};

class RadarWidget : public QFrame {
    Q_OBJECT
public:
    explicit RadarWidget(QWidget *parent = nullptr);
    ~RadarWidget() override;

    void set_devices(const QVector<RadarDevice> &devices);
    bool dark = true;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<RadarDevice> devices_;
    QVariantAnimation *sweep_anim_ = nullptr;
    QVariantAnimation *pulse_anim_ = nullptr;
    double sweep_angle_ = 0.0;
    double pulse_phase_ = 0.0;
};

#endif //NEZHAGUARD_RADAR_WIDGET_H