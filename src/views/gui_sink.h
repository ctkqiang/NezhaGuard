#ifndef NEZHAGUARD_GUI_SINK_H
#define NEZHAGUARD_GUI_SINK_H

#include <QObject>
#include <QPointer>
#include <QColor>
#include <QString>
#include <QTimer>
#include <QVector>
#include <memory>
#include <mutex>

#include "../utilities/logger.h"

class LogModel;

class GuiSink : public QObject, public Nezha::Log::ISink {
    Q_OBJECT

public:
    explicit GuiSink(LogModel *model, QObject *parent = nullptr);

    void write(Nezha::Log::Level lv, const char *line, std::size_t len) override;
    void flush() override;

private slots:
    void flush_batch();

private:
    static QColor level_color(const QString &level, const QString &message);

    struct Entry {
        QString timestamp;
        QString level;
        QString message;
        QColor color;
    };

    QPointer<LogModel> model_;
    QTimer *batch_timer_;
    std::mutex buf_mtx_;
    QVector<Entry> pending_;
};

#endif //NEZHAGUARD_GUI_SINK_H