#include "log_model.h"

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

void LogModel::append(const QString &timestamp, const QString &level, const QString &message) {
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
    if (level == QStringLiteral("Critical")) e.color = QColor("#f85149");
    else if (level == QStringLiteral("Error")) e.color = QColor("#f0883e");
    else if (level == QStringLiteral("Warn")) e.color = QColor("#d29922");
    else if (level == QStringLiteral("Info")) e.color = QColor("#3fb950");
    else if (level == QStringLiteral("Debug")) e.color = QColor("#58a6ff");
    else e.color = QColor("#6e7681");

    entries_.append(std::move(e));
    endInsertRows();
}
