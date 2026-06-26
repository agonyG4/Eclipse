#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QMultiHash>
#include <QFileSystemWatcher>
#include <QReadWriteLock>
#include <QTimer>
#include <memory>
#include <atomic>

struct DesktopEntryRecord {
    QString id;
    QString name;
    QString icon;
    QString exec;
    QString tryExec;
    QString startupWmClass;
    bool noDisplay = false;
    bool hidden = false;
};

struct DesktopEntrySnapshot {
    quint64 revision = 0;
    QString homeDir;
    QVector<DesktopEntryRecord> entries;
    QHash<QString, int> byDesktopId;            // desktopId -> index
    QMultiHash<QString, int> byStartupWmClass;  // WM_CLASS -> index
};

class DesktopEntryIndex : public QObject {
    Q_OBJECT
public:
    explicit DesktopEntryIndex(QObject *parent = nullptr);

    void initialize(const QString &customHome = QString());
    std::shared_ptr<const DesktopEntrySnapshot> getEntries() const;
    int revision() const;

signals:
    void indexUpdated();

private slots:
    void onDirectoryChanged(const QString &path);
    void triggerRebuild();

private:
    struct EntryBuild {
        DesktopEntryRecord rec;
    };
    void rebuildIndex();
    QStringList searchDirectories() const;

    mutable QReadWriteLock m_snapshotLock;
    std::shared_ptr<const DesktopEntrySnapshot> m_snapshot;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_debounceTimer = nullptr;
    QString m_homeDir;
};
