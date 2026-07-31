#ifndef NEZHAGUARD_LOG_MODEL_H
#define NEZHAGUARD_LOG_MODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QString>
#include <QVector>

class LogModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles { TimestampRole = Qt::UserRole + 1, LevelRole, MessageRole, ColorRole };

    struct Entry {
        QString timestamp;
        QString level;
        QString message;
        QColor color;
    };

    explicit LogModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void append(const QString &timestamp, const QString &level, const QString &message);

    [[nodiscard]] int total() const noexcept { return total_; }

private:
    static constexpr int kMaxEntries = 5000;
    QVector<Entry> entries_;
    int total_ = 0;
};

#endif //NEZHAGUARD_LOG_MODEL_H
