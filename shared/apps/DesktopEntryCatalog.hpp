#pragma once

#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QMultiHash>
#include <QObject>
#include <QReadWriteLock>
#include <QString>
#include <QTimer>
#include <QVector>
#include <memory>
#include <optional>

#include "apps/DesktopEntryParser.hpp"

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
    void onFileChanged(const QString &path);
    void rebuildIndex();

private:
    QStringList searchDirectories() const;
    void watchDirectories(const QStringList &directories);
    void watchFiles(const QStringList &files);

    mutable QReadWriteLock m_snapshotLock;
    std::shared_ptr<const DesktopEntrySnapshot> m_snapshot;
    QFileSystemWatcher m_watcher;
    QTimer m_debounceTimer;
    QString m_homeDir;
};
