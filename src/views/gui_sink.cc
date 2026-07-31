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

    int p1 = raw.indexOf(QStringLiteral("  [哪吒] ["));
    if (p1 > 0) {
        timestamp = raw.left(p1);
        int p2 = raw.indexOf(QChar(']'), p1 + 8);
        if (p2 > 0) {
            int p3 = raw.indexOf(QStringLiteral("  "), p2 + 1);
            message = p3 > 0 ? raw.mid(p3 + 2) : raw.mid(p2 + 2);
        } else {
            message = raw.mid(p1 + 8);
        }
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
