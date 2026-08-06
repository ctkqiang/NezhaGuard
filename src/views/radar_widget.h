//
// Created by 钟智强 on 2026/8/2.
//

#ifndef NEZHAGUARD_RADAR_WIDGET_H
#define NEZHAGUARD_RADAR_WIDGET_H

#include <QFrame>
#include <QString>
#include <QTimer>
#include <QVariantAnimation>
#include <QVector>
#include <QElapsedTimer>
#include <deque>

struct RadarDevice {
    QString ip;
    QString mac;
};

// small particle for ambient radar noise
struct RadarParticle {
    double angle;
    double dist;
    double alpha;
    double life;
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
    void resizeEvent(QResizeEvent *) override;

private:
    void spawn_particles();

    QVector<RadarDevice> devices_;
    QVariantAnimation *sweep_anim_ = nullptr;
    QVariantAnimation *pulse_anim_ = nullptr;
    QTimer *particle_timer_ = nullptr;
    double sweep_angle_ = 0.0;
    double pulse_phase_ = 0.0;
    int center_x_ = 0, center_y_ = 0, radius_ = 0;
    std::deque<RadarParticle> particles_;
    QElapsedTimer frame_timer_;
};

#endif //NEZHAGUARD_RADAR_WIDGET_H