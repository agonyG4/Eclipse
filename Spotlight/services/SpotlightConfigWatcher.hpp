#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QJsonObject>
#include <QString>

class SpotlightConfigWatcher : public QObject {
    Q_OBJECT
public:
    explicit SpotlightConfigWatcher(const QString &configPath, const QString &componentsPath,
                                    QObject *parent = nullptr);

    QJsonObject spotlightConfig() const { return m_spotlightConfig; }
    QJsonObject componentsConfig() const { return m_componentsConfig; }
    bool componentEnabled() const;
    bool weatherEnabled() const;

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
    QJsonObject m_spotlightConfig;
    QJsonObject m_componentsConfig;
    QFileSystemWatcher *m_watcher;
    QTimer *m_debounce;
};
