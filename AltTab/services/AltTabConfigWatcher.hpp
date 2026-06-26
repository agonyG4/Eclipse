#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QJsonObject>
#include <QString>

class AltTabConfigWatcher : public QObject {
    Q_OBJECT
public:
    explicit AltTabConfigWatcher(const QString &alttabConfigPath, const QString &componentsPath,
                                 QObject *parent = nullptr);

    QJsonObject alttabConfig() const { return m_alttabConfig; }
    QJsonObject componentsConfig() const { return m_componentsConfig; }
    bool componentEnabled() const;

signals:
    void configChanged();
    void componentToggled(bool enabled);

public slots:
    void refresh();

private:
    QJsonObject loadJsonFile(const QString &path) const;
    void addPathWithParents(const QString &path);

    QString m_configPath;
    QString m_componentsPath;
    QJsonObject m_alttabConfig;
    QJsonObject m_componentsConfig;
    QFileSystemWatcher *m_watcher;
    QTimer *m_debounce;
};
