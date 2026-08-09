#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonArray>
#include <QMultiHash>
#include <QObject>
#include <QReadWriteLock>
#include <QString>
#include <QTimer>
#include <QVector>
#include <memory>
#include <optional>

struct DesktopEntryRecord {
    QString desktopFileName;
    QString id;
    QString name;
    QString genericName;
    QString comment;
    QHash<QString, QString> localizedNames;
    QHash<QString, QString> localizedGenericNames;
    QHash<QString, QString> localizedComments;
    QString icon;
    QString exec;
    QString tryExec;
    QString startupWmClass;
    QString sourceFilePath;
    QStringList keywords;
    QStringList categories;
    QStringList onlyShowIn;
    QStringList notShowIn;
    bool terminal = false;
    bool noDisplay = false;
    bool hidden = false;
};

struct DesktopEntrySnapshot {
    quint64 revision = 0;
    QString homeDir;
    QVector<DesktopEntryRecord> entries;
    QHash<QString, int> byDesktopId;
    QHash<QString, int> byDesktopFileName;
    QMultiHash<QString, int> byStartupWmClass;
};

class DesktopEntryCatalog : public QObject {
    Q_OBJECT

public:
    explicit DesktopEntryCatalog(QObject *parent = nullptr);

    void initialize(const QString &customHome = QString());
    std::shared_ptr<const DesktopEntrySnapshot> snapshot() const;
    std::shared_ptr<const DesktopEntrySnapshot> getEntries() const { return snapshot(); }
    std::optional<DesktopEntryRecord> findByDesktopFileName(const QString &fileName) const;
    std::optional<DesktopEntryRecord> findByDesktopId(const QString &id) const;
    QJsonArray snapshotJson() const;
    QStringList watchedDirectories() const;
    int revision() const;

signals:
    void indexUpdated();

private slots:
    void onDirectoryChanged(const QString &path);
    void rebuildIndex();

private:
    QStringList searchDirectories() const;
    void watchDirectories(const QStringList &directories);

    mutable QReadWriteLock m_snapshotLock;
    std::shared_ptr<const DesktopEntrySnapshot> m_snapshot;
    QFileSystemWatcher m_watcher;
    QTimer m_debounceTimer;
    QString m_homeDir;
};
