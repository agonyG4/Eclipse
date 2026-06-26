#pragma once
#include <QObject>
#include <QString>
#include <QHash>
#include <QStringList>
#include <QFileSystemWatcher>
#include <QReadWriteLock>
#include <memory>
#include <optional>
#include <atomic>

struct SteamAppInfo {
    QString appId;
    QString name;
    QString installDir;
    QString libraryPath;
};

struct SteamAppSnapshot {
    quint64 revision = 0;
    QString homeDir;
    QString steamRoot;
    QStringList libraryPaths;
    QHash<QString, SteamAppInfo> apps;
};

class SteamMetadataIndex : public QObject {
    Q_OBJECT
public:
    explicit SteamMetadataIndex(QObject *parent = nullptr);

    void initialize(const QString &customHome = QString());
    std::optional<SteamAppInfo> getAppInfo(const QString &appId) const;
    QString findIconPath(const QString &appId) const;
    int revision() const;

signals:
    void indexUpdated();

private slots:
    void onDirectoryChanged(const QString &path);

private:
    void scanSteam();
    void parseLibraryFolders(const QString &steamPath, SteamAppSnapshot &snap);
    void scanLibrary(const QString &libPath, SteamAppSnapshot &snap);
    void parseManifest(const QString &manifestPath, const QString &libPath, SteamAppSnapshot &snap);

    mutable QReadWriteLock m_snapshotLock;
    std::shared_ptr<const SteamAppSnapshot> m_snapshot;
    QFileSystemWatcher *m_watcher = nullptr;
    QString m_homeDir;
};
