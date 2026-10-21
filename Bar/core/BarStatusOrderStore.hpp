#pragma once

#include <QObject>
#include <QStringList>

class BarStatusOrderStore final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList order READ order NOTIFY orderChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit BarStatusOrderStore(QString configFilePath = {}, QObject *parent = nullptr);

    QStringList order() const { return m_order; }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE void registerItem(const QString &key);
    Q_INVOKABLE bool commitVisibleOrder(const QStringList &visibleOrder);
    Q_INVOKABLE QString trayKey(const QString &id, const QString &addressKey) const;

    static QStringList defaultOrder();

signals:
    void orderChanged();
    void lastErrorChanged();
    void persistenceFailed(const QString &error);

private:
    void load();
    bool persist(const QStringList &order);
    void setLastError(const QString &error);

    QString m_configFilePath;
    QStringList m_order;
    QString m_lastError;
};
