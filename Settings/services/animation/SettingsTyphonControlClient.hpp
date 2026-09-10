#pragma once

#include <QObject>
#include <QVariantMap>

class SettingsTyphonControlClient final : public QObject {
    Q_OBJECT

public:
    explicit SettingsTyphonControlClient(QObject *parent = nullptr);

    bool request(const QString &command, const QVariantMap &arguments,
                 QVariantMap *result, QString *error);

private:
    QString discoverSocket(QString *error) const;
    static bool validInstanceName(const QString &value);
    static bool secureDirectory(const QString &path);
    static bool secureSocket(const QString &path);

    quint64 m_nextRequestId = 1;
};
