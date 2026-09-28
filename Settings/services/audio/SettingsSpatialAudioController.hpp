#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

class SettingsSpatialAudioController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool spatialAudioEnabled READ spatialAudioEnabled NOTIFY configurationChanged)
    Q_PROPERTY(bool spatialAudioConfigValid READ spatialAudioConfigValid NOTIFY configurationChanged)
    Q_PROPERTY(QString spatialAudioConfigError READ spatialAudioConfigError NOTIFY configurationChanged)

public:
    explicit SettingsSpatialAudioController(const QString &configPath = {},
                                            QObject *parent = nullptr);

    bool spatialAudioEnabled() const { return m_enabled; }
    bool spatialAudioConfigValid() const { return m_configValid; }
    QString spatialAudioConfigError() const
    {
        return m_persistenceError.isEmpty() ? m_configError : m_persistenceError;
    }

    Q_INVOKABLE bool setSpatialAudioEnabled(bool enabled);

signals:
    void configurationChanged();

private:
    void refresh(bool clearPersistenceError = false);
    void scheduleRefresh();
    void refreshWatchPaths();

    QString m_configPath;
    QString m_watchRoot;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    bool m_enabled = false;
    bool m_configValid = true;
    QString m_configError;
    QString m_persistenceError;
};
