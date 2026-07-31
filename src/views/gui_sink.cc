#include "gui_sink.h"
#include "log_model.h"

#include <QMetaObject>
#include <QString>

GuiSink::GuiSink(LogModel *model, QObject *parent) : QObject(parent), model_(model) {}

void GuiSink::write(Nezha::Log::Level lv, const char *line, std::size_t len) {
    if (!model_) return;

    QString raw = QString::fromUtf8(line, static_cast<int>(len));
    QString level = QString::fromUtf8(Nezha::Log::level_to_string(lv));

    QString timestamp;
    QString message;

    int at_pos = raw.indexOf(QStringLiteral("@ "));
    int msg_pos = raw.indexOf(QStringLiteral(": "), at_pos > 0 ? at_pos : 0);

    if (at_pos > 0 && msg_pos > at_pos) {
        timestamp = raw.mid(at_pos + 2, msg_pos - at_pos - 2);
        message = raw.mid(msg_pos + 2);
    } else {
        timestamp = QStringLiteral("-");
        message = raw;
    }

    QMetaObject::invokeMethod(
        model_.data(), "append", Qt::QueuedConnection,
        Q_ARG(QString, timestamp),
        Q_ARG(QString, level),
        Q_ARG(QString, message));
}
