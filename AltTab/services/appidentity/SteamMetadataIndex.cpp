#include "services/appidentity/SteamMetadataIndex.hpp"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QReadLocker>
#include <QWriteLocker>
#include <QRegularExpression>
#include <QDebug>

SteamMetadataIndex::SteamMetadataIndex(QObject *parent)
    : QObject(parent), m_snapshot(std::make_shared<const SteamAppSnapshot>())
{
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &SteamMetadataIndex::onDirectoryChanged);
}

void SteamMetadataIndex::initialize(const QString &customHome) {
    m_homeDir = customHome.isEmpty() ? QDir::homePath() : customHome;
    scanSteam();
}

std::optional<SteamAppInfo> SteamMetadataIndex::getAppInfo(const QString &appId) const {
    QReadLocker lock(&m_snapshotLock);
    auto it = m_snapshot->apps.find(appId);
    if (it != m_snapshot->apps.end()) {
        return it.value();
    }
    return std::nullopt;
}

QString SteamMetadataIndex::findIconPath(const QString &appId) const {
    // 1. Check client hicolor apps theme path
    QString iconThemePath = QStringLiteral("/usr/share/icons/hicolor/48x48/apps/steam_icon_") + appId + QStringLiteral(".png");
    if (QFileInfo::exists(iconThemePath))
        return iconThemePath;

    // 2. Check librarycache paths from snapshot
    QStringList candidates;
    QString steamRoot;
    QString homeDir;
    {
        QReadLocker lock(&m_snapshotLock);
        steamRoot = m_snapshot->steamRoot;
        homeDir = m_snapshot->homeDir;
    }
    if (!steamRoot.isEmpty()) {
        candidates << steamRoot + QStringLiteral("/appcache/librarycache/") + appId + QStringLiteral("/icon.png");
    }
    candidates << homeDir + QStringLiteral("/.steam/steam/appcache/librarycache/") + appId + QStringLiteral("/icon.png")
               << homeDir + QStringLiteral("/.steam/root/appcache/librarycache/") + appId + QStringLiteral("/icon.png")
               << homeDir + QStringLiteral("/.local/share/Steam/appcache/librarycache/") + appId + QStringLiteral("/icon.png");

    for (const auto &c : candidates) {
        if (QFileInfo::exists(c))
            return c;
    }
    return {};
}

int SteamMetadataIndex::revision() const {
    QReadLocker lock(&m_snapshotLock);
    return static_cast<int>(m_snapshot->revision);
}

void SteamMetadataIndex::scanSteam() {
    auto snap = std::make_shared<SteamAppSnapshot>();
    snap->homeDir = m_homeDir;
    snap->revision = m_snapshot ? m_snapshot->revision + 1 : 1;

    QStringList roots = {
        m_homeDir + QStringLiteral("/.steam/steam"),
        m_homeDir + QStringLiteral("/.steam/root"),
        m_homeDir + QStringLiteral("/.local/share/Steam")
    };

    for (const auto &r : roots) {
        if (QDir(r).exists()) {
            snap->steamRoot = r;
            break;
        }
    }

    if (snap->steamRoot.isEmpty()) {
        QWriteLocker lock(&m_snapshotLock);
        m_snapshot = std::move(snap);
        return;
    }

    snap->libraryPaths << snap->steamRoot;
    parseLibraryFolders(snap->steamRoot, *snap);

    for (const auto &lib : snap->libraryPaths) {
        scanLibrary(lib, *snap);
    }

    {
        QWriteLocker lock(&m_snapshotLock);
        m_snapshot = std::move(snap);
    }
    emit indexUpdated();
}

void SteamMetadataIndex::parseLibraryFolders(const QString &steamPath, SteamAppSnapshot &snap) {
    QFile file(steamPath + QStringLiteral("/steamapps/libraryfolders.vdf"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream ts(&file);
    static const QRegularExpression re(QStringLiteral("\"path\"\\s+\"([^\"]+)\""), QRegularExpression::CaseInsensitiveOption);
    while (!ts.atEnd()) {
        QString line = ts.readLine();
        auto match = re.match(line);
        if (match.hasMatch()) {
            QString path = match.captured(1);
            if (QDir(path).exists() && !snap.libraryPaths.contains(path)) {
                snap.libraryPaths << path;
            }
        }
    }
}

void SteamMetadataIndex::scanLibrary(const QString &libPath, SteamAppSnapshot &snap) {
    QString appsDir = libPath + QStringLiteral("/steamapps");
    QDir dir(appsDir);
    if (!dir.exists())
        return;

    m_watcher->addPath(appsDir);

    const auto files = dir.entryList({QStringLiteral("appmanifest_*.acf")}, QDir::Files);
    for (const auto &f : files) {
        parseManifest(dir.absoluteFilePath(f), libPath, snap);
    }
}

void SteamMetadataIndex::parseManifest(const QString &manifestPath, const QString &libPath, SteamAppSnapshot &snap) {
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream ts(&file);
    SteamAppInfo app;
    app.libraryPath = libPath;

    static const QRegularExpression idRe(QStringLiteral("\"appid\"\\s+\"([^\"]+)\""), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression nameRe(QStringLiteral("\"name\"\\s+\"([^\"]+)\""), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression dirRe(QStringLiteral("\"installdir\"\\s+\"([^\"]+)\""), QRegularExpression::CaseInsensitiveOption);

    while (!ts.atEnd()) {
        QString line = ts.readLine();
        auto mId = idRe.match(line);
        if (mId.hasMatch()) {
            app.appId = mId.captured(1);
            continue;
        }
        auto mName = nameRe.match(line);
        if (mName.hasMatch()) {
            app.name = mName.captured(1);
            continue;
        }
        auto mDir = dirRe.match(line);
        if (mDir.hasMatch()) {
            app.installDir = mDir.captured(1);
            continue;
        }
    }

    if (!app.appId.isEmpty()) {
        snap.apps.insert(app.appId, app);
    }
}

void SteamMetadataIndex::onDirectoryChanged(const QString &path) {
    Q_UNUSED(path);
    scanSteam();
    emit indexUpdated();
}
