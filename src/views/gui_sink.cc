#include "gui_sink.h"
#include "log_model.h"
#include "theme.h"

#include <QMetaObject>
#include <QRegularExpression>
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

    QColor color = level_color(level, message);

    QMetaObject::invokeMethod(
        model_.data(), "append", Qt::QueuedConnection,
        Q_ARG(QString, timestamp),
        Q_ARG(QString, level),
        Q_ARG(QString, message),
        Q_ARG(QColor, color));
}

QColor GuiSink::level_color(const QString &level, const QString &message) {
    if (level.startsWith(QStringLiteral("CRIT")) || level == QStringLiteral("Critical"))
        return QColor(Theme::Red);
    if (level.startsWith(QStringLiteral("ERR")) || level == QStringLiteral("Error"))
        return QColor(Theme::Orange);
    if (level.startsWith(QStringLiteral("WARN")) || level == QStringLiteral("Warn"))
        return QColor(Theme::Orange);
    if (level.startsWith(QStringLiteral("INFO")) || level == QStringLiteral("Info"))
        return QColor(Theme::PinkDeep);
    if (level.startsWith(QStringLiteral("DEB")) || level == QStringLiteral("Debug"))
        return QColor(Theme::CyanLight);
    if (level.startsWith(QStringLiteral("TRA")) || level == QStringLiteral("Trace"))
        return QColor(Theme::Grey);

    if (message.contains(QStringLiteral("隔离")) || message.contains(QStringLiteral("quarantine")))
        return QColor(Theme::PinkDeep);
    if (message.contains(QStringLiteral("拦截")) || message.contains(QStringLiteral("blocked")))
        return QColor(Theme::Red);
    if (message.contains(QStringLiteral("已启动")) || message.contains(QStringLiteral("started")))
        return QColor(Theme::Green);
    if (message.contains(QStringLiteral("Tor")))
        return QColor(Theme::Purple);
    if (message.contains(QStringLiteral("蜜罐")) || message.contains(QStringLiteral("honeypot")))
        return QColor(Theme::PinkDeep);

    return QColor(Theme::CyanLight);
}
