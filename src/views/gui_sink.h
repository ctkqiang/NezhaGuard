#ifndef NEZHAGUARD_GUI_SINK_H
#define NEZHAGUARD_GUI_SINK_H

#include <QObject>
#include <QPointer>
#include <QColor>
#include <QString>
#include <memory>

#include "../utilities/logger.h"

class LogModel;

class GuiSink : public QObject, public Nezha::Log::ISink {
    Q_OBJECT

public:
    explicit GuiSink(LogModel *model, QObject *parent = nullptr);

    void write(Nezha::Log::Level lv, const char *line, std::size_t len) override;
    void flush() override {}

private:
    static QColor level_color(const QString &level, const QString &message);
    QPointer<LogModel> model_;
};

#endif //NEZHAGUARD_GUI_SINK_H
