#pragma once

#include <QString>
#include <QStringList>

class DockConfigPersistence {
public:
    explicit DockConfigPersistence(const QString &configPath);
    virtual ~DockConfigPersistence() = default;

    QString configPath() const { return m_configPath; }
    virtual bool writePins(const QStringList &pins, QString *errorOut = nullptr);

private:
    QString m_configPath;
};
