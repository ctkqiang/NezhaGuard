#include "gui_sink.h"
#include "log_model.h"

#include <QMetaObject>
#include <QString>

GuiSink::GuiSink(LogModel *model, QObject *parent) : QObject(parent), model_(model) {}

void GuiSink::write(Nezha::Log::Level lv, const char *line, std::size_t len) {
    if (!model_) return;

    QString raw = QString::fromUtf8(line, static_cast<int>(len));
    QString level = QString::fromUtf8(Nezha::Log::level_to_string(lv)).trimmed();
    if (level.isEmpty()) level = QStringLiteral("INFO");

    QString timestamp;
    QString message;

    int bracket = raw.indexOf(QStringLiteral("  ["));
    if (bracket > 0) {
        timestamp = raw.left(bracket);
        int msg_start = raw.indexOf(QStringLiteral("]  "), bracket);
        message = msg_start > 0 ? raw.mid(msg_start + 3) : raw.mid(bracket + 2);
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
