#include "apps/DesktopEntryCatalog.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QReadLocker>
#include <QSet>
#include <QWriteLocker>

#include <algorithm>

namespace {

constexpr int kMaximumScanDepth = 5;
constexpr int kMaximumFilesPerRoot = 10000;

QStringList uniquePaths(const QStringList &paths)
{
    QStringList result;
    QSet<QString> seen;
    for (const QString &path : paths) {
        const QString clean = QDir::cleanPath(path);
        if (!clean.isEmpty() && !seen.contains(clean)) {
            seen.insert(clean);
            result.append(clean);
        }
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

struct CollectedRoot {
    QStringList files;
    QStringList directories;
};

CollectedRoot collectDesktopFiles(const QString &root)
{
    CollectedRoot result;
    const QString absoluteRoot = QFileInfo(root).absoluteFilePath();
    if (!QDir(absoluteRoot).exists())
        return result;

    QVector<QPair<QString, int>> pending;
    pending.append({absoluteRoot, 0});
    qsizetype nextDirectory = 0;
    bool fileLimitReached = false;
    while (nextDirectory < pending.size() && !fileLimitReached) {
        const auto [currentPath, depth] = pending.at(nextDirectory++);
        if (depth > kMaximumScanDepth)
            continue;

        result.directories.append(currentPath);
        QFileInfoList children = QDir(currentPath).entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot, QDir::NoSort);
        std::sort(children.begin(), children.end(), [](const QFileInfo &left, const QFileInfo &right) {
            return left.absoluteFilePath() < right.absoluteFilePath();
        });

        for (const QFileInfo &child : children) {
            if (child.isDir()) {
                if (!child.isSymLink())
                    pending.append({child.absoluteFilePath(), depth + 1});
                continue;
            }
            if (child.isFile() && child.suffix() == QStringLiteral("desktop")) {
                result.files.append(child.absoluteFilePath());
                if (result.files.size() == kMaximumFilesPerRoot) {
                    fileLimitReached = true;
                    break;
                }
            }
        }
    }

    std::sort(result.files.begin(), result.files.end());
    return result;
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
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &DesktopEntryCatalog::onFileChanged);
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

QJsonArray DesktopEntryCatalog::snapshotJson() const
{
    QJsonArray result;
    const auto current = snapshot();
    const auto listToJson = [](const QStringList &values) {
        QJsonArray array;
        for (const QString &value : values)
            array.append(value);
        return array;
    };
    const auto hashToJson = [](const QHash<QString, QString> &values) {
        QJsonObject object;
        for (auto it = values.constBegin(); it != values.constEnd(); ++it)
            object.insert(it.key(), it.value());
        return object;
    };
    const auto hashListToJson = [](const QHash<QString, QStringList> &values) {
        QJsonObject object;
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            QJsonArray array;
            for (const QString &value : it.value())
                array.append(value);
            object.insert(it.key(), array);
        }
        return object;
    };
    const auto actionsToJson = [](const QVector<DesktopEntryAction> &actions) {
        QJsonArray array;
        for (const DesktopEntryAction &action : actions) {
            QJsonObject localized;
            for (auto it = action.localizedNames.constBegin(); it != action.localizedNames.constEnd(); ++it)
                localized.insert(it.key(), it.value());
            array.append(QJsonObject{
                {QStringLiteral("id"), action.id},
                {QStringLiteral("name"), action.name},
                {QStringLiteral("localized_names"), localized},
                {QStringLiteral("icon"), action.icon},
                {QStringLiteral("exec"), action.exec}
            });
        }
        return array;
    };

    for (const DesktopEntryRecord &entry : current->entries) {
        result.append(QJsonObject{
            {QStringLiteral("id"), entry.id},
            {QStringLiteral("name"), entry.name},
            {QStringLiteral("generic_name"), entry.genericName},
            {QStringLiteral("comment"), entry.comment},
            {QStringLiteral("localized_names"), hashToJson(entry.localizedNames)},
            {QStringLiteral("localized_generic_names"), hashToJson(entry.localizedGenericNames)},
            {QStringLiteral("localized_comments"), hashToJson(entry.localizedComments)},
            {QStringLiteral("localized_keywords"), hashListToJson(entry.localizedKeywords)},
            {QStringLiteral("icon"), entry.icon},
            {QStringLiteral("exec"), entry.exec},
            {QStringLiteral("try_exec"), entry.tryExec},
            {QStringLiteral("keywords"), listToJson(entry.keywords)},
            {QStringLiteral("categories"), listToJson(entry.categories)},
            {QStringLiteral("path"), entry.sourceFilePath},
            {QStringLiteral("startup_wm_class"), entry.startupWmClass},
            {QStringLiteral("desktop_file_name"), entry.desktopFileName},
            {QStringLiteral("terminal"), entry.terminal},
            {QStringLiteral("hidden"), entry.hidden},
            {QStringLiteral("no_display"), entry.noDisplay},
            {QStringLiteral("only_show_in"), listToJson(entry.onlyShowIn)},
            {QStringLiteral("not_show_in"), listToJson(entry.notShowIn)},
            {QStringLiteral("actions"), actionsToJson(entry.actions)}
        });
    }
    return result;
}

QStringList DesktopEntryCatalog::watchedDirectories() const
{
    return m_watcher.directories();
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

void DesktopEntryCatalog::watchFiles(const QStringList &files)
{
    const QStringList watched = m_watcher.files();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);

    const QStringList unique = uniquePaths(files);
    if (!unique.isEmpty())
        m_watcher.addPaths(unique);
}

void DesktopEntryCatalog::rebuildIndex()
{
    const QStringList roots = searchDirectories();
    auto next = std::make_shared<DesktopEntrySnapshot>();
    next->homeDir = m_homeDir;
    next->revision = snapshot()->revision + 1;

    QStringList watchPaths;
    QStringList filePaths;
    QSet<QString> consumedIds;
    for (const QString &root : roots) {
        watchPaths.append(root);
        const CollectedRoot collected = collectDesktopFiles(root);
        watchPaths.append(collected.directories);
        filePaths.append(collected.files);
        for (const QString &sourcePath : collected.files) {
            const auto parsed = DesktopEntryParser::parse(sourcePath, root);
            if (!parsed.has_value())
                continue;

            const DesktopEntryRecord &record = parsed.value();
            if (consumedIds.contains(record.id))
                continue;
            consumedIds.insert(record.id);
            if (record.hidden)
                continue;

            const int index = static_cast<int>(next->entries.size());
            next->entries.append(record);
            next->byDesktopFileName.insert(record.desktopFileName, index);
            next->byDesktopId.insert(record.id, index);
            if (!record.startupWmClass.isEmpty())
                next->byStartupWmClass.insert(record.startupWmClass, index);
        }
    }

    watchDirectories(watchPaths);
    watchFiles(filePaths);
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

void DesktopEntryCatalog::onFileChanged(const QString &path)
{
    Q_UNUSED(path);
    m_debounceTimer.start();
}
