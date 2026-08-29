#pragma once

#include <QFileSystemWatcher>
#include "dock/DockConfig.hpp"
#include <QObject>
#include <QStringList>
#include <QTimer>

class DockConfigWatcher final : public QObject {
    Q_OBJECT

public:
    explicit DockConfigWatcher(const QString &configPath, const QString &componentsPath,
                               QObject *parent = nullptr);

    DockConfig config() const { return m_config; }
    bool componentEnabled() const;
    QString configPath() const { return m_configPath; }
    QString componentsPath() const { return m_componentsPath; }
    quint64 revision() const { return m_revision; }
    QString lastError() const { return m_lastError; }

signals:
    void configChanged();
    void componentToggled(bool enabled);

public slots:
    void refresh();

private:
    struct JsonResult {
        QJsonObject object;
        QString error;
    };

    JsonResult loadJsonFile(const QString &path) const;
    void addPathWithParents(const QString &path);

    QString m_configPath;
    QString m_componentsPath;
    DockConfig m_config;
    QJsonObject m_componentsConfig;
    QString m_lastError;
    quint64 m_revision = 0;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
