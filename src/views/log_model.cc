#include "log_model.h"
#include "theme.h"

LogModel::LogModel(QObject *parent) : QAbstractListModel(parent) {
    entries_.reserve(kMaxEntries);
}

int LogModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(entries_.size());
}

QVariant LogModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size()) return {};

    const auto &e = entries_[index.row()];
    switch (role) {
        case Qt::DisplayRole: return e.message;
        case TimestampRole: return e.timestamp;
        case LevelRole: return e.level;
        case MessageRole: return e.message;
        case ColorRole: return e.color;
        default: return {};
    }
}

QHash<int, QByteArray> LogModel::roleNames() const {
    return {
        {TimestampRole, "timestamp"},
        {LevelRole, "level"},
        {MessageRole, "message"},
        {ColorRole, "color"},
        {Qt::DisplayRole, "display"},
    };
}

QColor LogModel::default_color(const QString &level) {
    if (level.startsWith(QStringLiteral("CRIT")) || level == QStringLiteral("Critical"))
        return QColor(Theme::Red);
    if (level.startsWith(QStringLiteral("ERR")) || level == QStringLiteral("Error"))
        return QColor(Theme::Orange);
    if (level.startsWith(QStringLiteral("WARN")) || level == QStringLiteral("Warn"))
        return QColor(Theme::Orange);
    if (level.startsWith(QStringLiteral("INFO")) || level == QStringLiteral("Info"))
        return QColor(Theme::PinkDeep);
    if (level.startsWith(QStringLiteral("DEB")) || level == QStringLiteral("Debug"))
        return QColor(Theme::PinkLight);
    return QColor(Theme::Grey);
}

void LogModel::append(const QString &timestamp, const QString &level, const QString &message) {
    append(timestamp, level, message, default_color(level));
}

void LogModel::append(const QString &timestamp, const QString &level, const QString &message,
                      const QColor &color) {
    ++total_;
    if (entries_.size() >= kMaxEntries) {
        beginRemoveRows(QModelIndex(), 0, 0);
        entries_.pop_front();
        endRemoveRows();
    }
    int pos = static_cast<int>(entries_.size());
    beginInsertRows(QModelIndex(), pos, pos);

    Entry e{};
    e.timestamp = timestamp;
    e.level = level;
    e.message = message;
    e.color = color;

    entries_.append(std::move(e));
    endInsertRows();
}

void LogModel::clear() {
    if (entries_.isEmpty()) return;
    beginRemoveRows(QModelIndex(), 0, entries_.size() - 1);
    entries_.clear();
    total_ = 0;
    endRemoveRows();
}
