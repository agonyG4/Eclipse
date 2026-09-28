#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

class SpatialAudioConfigWatcher final : public QObject {
    Q_OBJECT

  public:
    explicit SpatialAudioConfigWatcher(const QString &configPath,
                                       QObject *parent = nullptr);

    static QString defaultConfigPath();

    bool enabled() const { return m_enabled; }
    QString errorString() const { return m_errorString; }
    QString configPath() const { return m_configPath; }

  public slots:
    void refresh();

  signals:
    void enabledChanged(bool enabled);
    void errorChanged(const QString &error);

  private:
    void scheduleRefresh();
    void refreshWatchPaths();

    QString m_configPath;
    QString m_watchRoot;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    bool m_enabled = false;
    QString m_errorString;
};
