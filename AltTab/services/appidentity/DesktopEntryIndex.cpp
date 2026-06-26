#include "services/appidentity/DesktopEntryIndex.hpp"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QReadLocker>
#include <QWriteLocker>
#include <QDebug>

DesktopEntryIndex::DesktopEntryIndex(QObject *parent)
    : QObject(parent), m_snapshot(std::make_shared<const DesktopEntrySnapshot>())
{
    m_watcher = new QFileSystemWatcher(this);
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(300);

    connect(m_debounceTimer, &QTimer::timeout, this, &DesktopEntryIndex::rebuildIndex);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &DesktopEntryIndex::onDirectoryChanged);
}

void DesktopEntryIndex::initialize(const QString &customHome) {
    m_homeDir = customHome.isEmpty() ? QDir::homePath() : customHome;
    rebuildIndex();
}

std::shared_ptr<const DesktopEntrySnapshot> DesktopEntryIndex::getEntries() const {
    QReadLocker lock(&m_snapshotLock);
    return m_snapshot;
}

int DesktopEntryIndex::revision() const {
    QReadLocker lock(&m_snapshotLock);
    return static_cast<int>(m_snapshot->revision);
}

QStringList DesktopEntryIndex::searchDirectories() const {
    QStringList dirs;
    dirs << m_homeDir + QStringLiteral("/.local/share/applications")
         << QStringLiteral("/usr/share/applications")
         << QStringLiteral("/usr/local/share/applications")
         << QStringLiteral("/var/lib/flatpak/exports/share/applications")
         << m_homeDir + QStringLiteral("/.local/share/flatpak/exports/share/applications");
    return dirs;
}

void DesktopEntryIndex::rebuildIndex() {
    auto snap = std::make_shared<DesktopEntrySnapshot>();
    snap->homeDir = m_homeDir;
    snap->revision = m_snapshot ? m_snapshot->revision + 1 : 1;

    const QStringList dirs = searchDirectories();
    QHash<QString, int> idIndex;

    for (const auto &dirPath : dirs) {
        if (QDir(dirPath).exists()) {
            m_watcher->addPath(dirPath);
            QDir dir(dirPath);
            const auto files = dir.entryList({QStringLiteral("*.desktop")}, QDir::Files);
            for (const auto &file : files) {
                const QString fullPath = dir.absoluteFilePath(file);
                QFile f(fullPath);
                if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                    continue;

                DesktopEntryRecord info;
                const QString desktopId = file.chopped(8);

                QTextStream in(&f);
                bool inDesktopEntrySection = false;
                while (!in.atEnd()) {
                    QString line = in.readLine().trimmed();
                    if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
                        inDesktopEntrySection = (line == QStringLiteral("[Desktop Entry]"));
                        continue;
                    }
                    if (!inDesktopEntrySection)
                        continue;

                    if (line.startsWith(QStringLiteral("Name=")))
                        info.name = line.mid(5);
                    else if (line.startsWith(QStringLiteral("Icon=")))
                        info.icon = line.mid(5);
                    else if (line.startsWith(QStringLiteral("Exec=")))
                        info.exec = line.mid(5);
                    else if (line.startsWith(QStringLiteral("TryExec=")))
                        info.tryExec = line.mid(8);
                    else if (line.startsWith(QStringLiteral("StartupWMClass=")))
                        info.startupWmClass = line.mid(15);
                    else if (line.startsWith(QStringLiteral("NoDisplay=")) && line.mid(9) == QStringLiteral("true"))
                        info.noDisplay = true;
                    else if (line.startsWith(QStringLiteral("Hidden=")) && line.mid(7) == QStringLiteral("true"))
                        info.hidden = true;
                }

                if (!info.noDisplay && !info.hidden) {
                    const int idx = snap->entries.size();
                    snap->entries.append(info);
                    snap->byDesktopId.insert(desktopId, idx);
                    if (!info.startupWmClass.isEmpty())
                        snap->byStartupWmClass.insert(info.startupWmClass, idx);
                }
            }
        }
    }

    {
        QWriteLocker lock(&m_snapshotLock);
        m_snapshot = std::move(snap);
    }
    emit indexUpdated();
}

void DesktopEntryIndex::onDirectoryChanged(const QString &path) {
    Q_UNUSED(path);
    m_debounceTimer->start();
}

void DesktopEntryIndex::triggerRebuild() {
    m_debounceTimer->start();
}
