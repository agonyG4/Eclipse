#include "apps/DesktopEntryCatalog.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QReadLocker>
#include <QSet>
#include <QTextStream>
#include <QWriteLocker>

namespace {

bool parseBoolean(const QString &value)
{
    return value.trimmed().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
}

QStringList uniquePaths(const QStringList &paths)
{
    QStringList result;
    for (const QString &path : paths) {
        const QString clean = QDir::cleanPath(path);
        if (!clean.isEmpty() && !result.contains(clean))
            result.append(clean);
    }
    return result;
}

QString nearestExistingDirectory(const QString &path)
{
    QString candidate = QFileInfo(path).absoluteFilePath();
    while (!QDir(candidate).exists()) {
        const QString parent = QFileInfo(candidate).absoluteDir().absolutePath();
        if (parent == candidate || candidate == QDir::rootPath())
            return {};
        candidate = parent;
    }
    return candidate;
}

} // namespace

DesktopEntryCatalog::DesktopEntryCatalog(QObject *parent)
    : QObject(parent), m_snapshot(std::make_shared<const DesktopEntrySnapshot>())
{
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(250);
    connect(&m_debounceTimer, &QTimer::timeout, this, &DesktopEntryCatalog::rebuildIndex);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &DesktopEntryCatalog::onDirectoryChanged);
}

void DesktopEntryCatalog::initialize(const QString &customHome)
{
    m_homeDir = customHome.isEmpty() ? QDir::homePath() : QDir::cleanPath(customHome);
    rebuildIndex();
}

std::shared_ptr<const DesktopEntrySnapshot> DesktopEntryCatalog::snapshot() const
{
    QReadLocker lock(&m_snapshotLock);
    return m_snapshot;
}

std::optional<DesktopEntryRecord> DesktopEntryCatalog::findByDesktopFileName(const QString &fileName) const
{
    const auto current = snapshot();
    const auto it = current->byDesktopFileName.constFind(fileName);
    if (it == current->byDesktopFileName.constEnd())
        return std::nullopt;
    return current->entries.at(it.value());
}

std::optional<DesktopEntryRecord> DesktopEntryCatalog::findByDesktopId(const QString &id) const
{
    const auto current = snapshot();
    const auto it = current->byDesktopId.constFind(id);
    if (it == current->byDesktopId.constEnd())
        return std::nullopt;
    return current->entries.at(it.value());
}

int DesktopEntryCatalog::revision() const
{
    return static_cast<int>(snapshot()->revision);
}

QStringList DesktopEntryCatalog::searchDirectories() const
{
    const auto env = QProcessEnvironment::systemEnvironment();
    const QString dataHome = m_homeDir == QDir::homePath()
        ? env.value(QStringLiteral("XDG_DATA_HOME"), m_homeDir + QStringLiteral("/.local/share"))
        : m_homeDir + QStringLiteral("/.local/share");
    QStringList result{dataHome,
                       m_homeDir + QStringLiteral("/.local/share/flatpak/exports/share")};

    const QStringList dataDirs = env.value(QStringLiteral("XDG_DATA_DIRS"),
                                           QStringLiteral("/usr/local/share:/usr/share"))
                                     .split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &dataDir : dataDirs)
        result.append(dataDir);
    result.append(QStringLiteral("/var/lib/flatpak/exports/share"));

    QStringList applications;
    for (const QString &base : uniquePaths(result))
        applications.append(QDir(base).filePath(QStringLiteral("applications")));
    return uniquePaths(applications);
}

void DesktopEntryCatalog::watchDirectories(const QStringList &directories)
{
    const QStringList watched = m_watcher.directories();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);

    QSet<QString> paths;
    for (const QString &directory : directories) {
        const QString watchPath = nearestExistingDirectory(directory);
        if (!watchPath.isEmpty() && !paths.contains(watchPath)) {
            paths.insert(watchPath);
            m_watcher.addPath(watchPath);
        }
    }
}

void DesktopEntryCatalog::rebuildIndex()
{
    const QStringList directories = searchDirectories();
    auto next = std::make_shared<DesktopEntrySnapshot>();
    next->homeDir = m_homeDir;
    next->revision = snapshot()->revision + 1;

    for (const QString &directory : directories) {
        const QDir dir(directory);
        if (!dir.exists())
            continue;

        const QStringList files = dir.entryList({QStringLiteral("*.desktop")},
                                                QDir::Files | QDir::Readable, QDir::Name);
        for (const QString &fileName : files) {
            if (next->byDesktopFileName.contains(fileName))
                continue;

            const QString sourcePath = dir.absoluteFilePath(fileName);
            QFile file(sourcePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            DesktopEntryRecord record;
            record.desktopFileName = fileName;
            record.id = fileName.chopped(QStringLiteral(".desktop").size());
            record.sourceFilePath = sourcePath;

            QTextStream stream(&file);
            bool inDesktopEntry = false;
            while (!stream.atEnd()) {
                const QString line = stream.readLine().trimmed();
                if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
                    inDesktopEntry = line == QStringLiteral("[Desktop Entry]");
                    continue;
                }
                if (!inDesktopEntry)
                    continue;

                const int separator = static_cast<int>(line.indexOf(QLatin1Char('=')));
                if (separator <= 0)
                    continue;
                const QString key = line.left(separator);
                const QString value = line.mid(separator + 1);
                if (key == QStringLiteral("Name"))
                    record.name = value;
                else if (key == QStringLiteral("Icon"))
                    record.icon = value;
                else if (key == QStringLiteral("Exec"))
                    record.exec = value;
                else if (key == QStringLiteral("TryExec"))
                    record.tryExec = value;
                else if (key == QStringLiteral("StartupWMClass"))
                    record.startupWmClass = value;
                else if (key == QStringLiteral("NoDisplay"))
                    record.noDisplay = parseBoolean(value);
                else if (key == QStringLiteral("Hidden"))
                    record.hidden = parseBoolean(value);
            }

            const int index = static_cast<int>(next->entries.size());
            next->entries.append(record);
            next->byDesktopFileName.insert(record.desktopFileName, index);
            next->byDesktopId.insert(record.id, index);
            if (!record.startupWmClass.isEmpty())
                next->byStartupWmClass.insert(record.startupWmClass, index);
        }
    }

    watchDirectories(directories);
    {
        QWriteLocker lock(&m_snapshotLock);
        m_snapshot = std::move(next);
    }
    emit indexUpdated();
}

void DesktopEntryCatalog::onDirectoryChanged(const QString &path)
{
    Q_UNUSED(path);
    m_debounceTimer.start();
}
