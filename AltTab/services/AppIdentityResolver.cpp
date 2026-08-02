#include "services/AppIdentityResolver.hpp"
#include "services/appidentity/ProcessInspector.hpp"
#include "services/appidentity/WineExecutableResolver.hpp"
#include <QRunnable>
#include <QIcon>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>

class ResolveTask : public QRunnable {
public:
    ResolveTask(AppIdentityResolver *resolver, const WindowIdentityInput &input, int generation)
        : m_resolver(resolver), m_input(input), m_generation(generation) {}

    void run() override {
        AppIdentity identity = m_resolver->resolveDeep(m_input);
        QMetaObject::invokeMethod(m_resolver, "onDeepResolved",
                                  Qt::QueuedConnection,
                                  Q_ARG(WindowIdentityInput, m_input),
                                  Q_ARG(AppIdentity, identity),
                                  Q_ARG(int, m_generation));
    }

private:
    AppIdentityResolver *m_resolver;
    WindowIdentityInput m_input;
    int m_generation;
};

AppIdentityResolver::AppIdentityResolver(QObject *parent)
    : QObject(parent), m_procRoot(QStringLiteral("/proc"))
{
    qRegisterMetaType<WindowIdentityInput>();
    qRegisterMetaType<AppIdentity>();
    m_desktopIndex = new DesktopEntryIndex(this);
    m_steamIndex = new SteamMetadataIndex(this);
}

AppIdentityResolver::~AppIdentityResolver() {
    QThreadPool::globalInstance()->waitForDone();
}

void AppIdentityResolver::initialize(const QString &customHome, const QString &customProc) {
    m_procRoot = customProc.isEmpty() ? QStringLiteral("/proc") : customProc;
    m_desktopIndex->initialize(customHome);
    m_steamIndex->initialize(customHome);
}

AppIdentity AppIdentityResolver::resolveSync(const WindowIdentityInput &input) {
    const QString fingerprint = input.metadataFingerprint.isEmpty()
        ? (input.className + QLatin1Char('|') + input.initialClass + QLatin1Char('|')
           + input.title + QLatin1Char('|') + input.initialTitle).toLower()
        : input.metadataFingerprint;
    const QString cacheKey = input.address + QLatin1Char(':') + QString::number(input.pid)
                              + QLatin1Char(':') + fingerprint
                              + QLatin1Char(':') + QString::number(input.themeRevision)
                              + QLatin1Char(':') + QString::number(input.desktopIndexRevision)
                              + QLatin1Char(':') + QString::number(input.steamIndexRevision);
    AppIdentity identity;
    if (m_cache.lookup(cacheKey, identity)) {
        return identity;
    }

    identity = resolveFast(input);
    identity.windowId = input.address;
    identity.pid = input.pid;
    identity.openGeneration = input.openGeneration;
    identity.metadataFingerprint = fingerprint;
    identity.themeRevision = input.themeRevision;
    identity.desktopIndexRevision = input.desktopIndexRevision;
    identity.steamIndexRevision = input.steamIndexRevision;
    if (!identity.iconPending) {
        m_cache.insert(cacheKey, identity);
    }
    return identity;
}

void AppIdentityResolver::resolveAsync(const WindowIdentityInput &input, int generation) {
    AppIdentity identity = resolveSync(input);
    if (identity.iconPending) {
        // Run deep resolution in worker pool
        QThreadPool::globalInstance()->start(new ResolveTask(this, input, generation));
    } else {
        emit identityResolved(input.address, identity);
    }
}

AppIdentity AppIdentityResolver::resolveFast(const WindowIdentityInput &input) {
    AppIdentity result;
    result.stableKey = input.address;
    result.showFallbackText = true;
    result.source = QStringLiteral("fallback");

    // 1. Check aliases
    result = resolveAliases(input);
    if (!result.iconName.isEmpty()) {
        result.source = QStringLiteral("alias");
        return result;
    }

    // 2. Check Steam AppID fast matching (e.g. class is steam_app_X)
    result = resolveSteamAppId(input);
    if (!result.iconName.isEmpty()) {
        result.source = QStringLiteral("steam-appid");
        return result;
    }

    // 3. Detect if deep resolution is needed (Wine, Proton, Steam pending, etc.)
    const QString checkText = (input.className + QLatin1Char(' ') + input.initialClass
                               + QLatin1Char(' ') + input.title + QLatin1Char(' ') + input.initialTitle).toLower();
    const bool needsDeep = checkText.contains(QStringLiteral(".exe"))
                        || checkText.contains(QStringLiteral("wine"))
                        || checkText.contains(QStringLiteral("proton"))
                        || checkText.contains(QStringLiteral("pressure-vessel"))
                        || checkText.contains(QStringLiteral("steam_app_"))
                        || checkText.contains(QStringLiteral("unidentified game"));

    if (needsDeep) {
        result.iconPending = true;
        result.showFallbackText = false;
        result.source = QStringLiteral("deep-pending");
        return result;
    }

    // 4. Desktop entry matching
    result = resolveDesktopEntry(input);
    if (!result.iconName.isEmpty()) {
        result.source = QStringLiteral("desktop-entry");
        return result;
    }

    // 5. Fallback theme
    result = resolveThemeFallback(input);
    if (!result.iconName.isEmpty()) {
        result.source = QStringLiteral("theme");
        return result;
    }

    return result;
}

AppIdentity AppIdentityResolver::resolveDeep(const WindowIdentityInput &input) {
    AppIdentity result;
    result.stableKey = input.address;
    result.windowId = input.address;
    result.pid = input.pid;
    result.openGeneration = input.openGeneration;
    result.metadataFingerprint = input.metadataFingerprint;
    result.themeRevision = input.themeRevision;
    result.desktopIndexRevision = input.desktopIndexRevision;
    result.steamIndexRevision = input.steamIndexRevision;
    result.showFallbackText = true;
    result.source = QStringLiteral("deep-resolved");

    // Gather info from Proc
    ProcessInspector::ProcInfo proc = ProcessInspector::inspectProcess(input.pid, m_procRoot);
    QVector<qint64> ancestors = ProcessInspector::getAncestors(input.pid, 5, m_procRoot);

    // 1. Try to find Steam AppID from env / cmdline / cwd / ancestors
    QString steamAppId;
    
    // Check main process env
    if (proc.env.contains(QStringLiteral("SteamAppId"))) {
        steamAppId = proc.env.value(QStringLiteral("SteamAppId"));
    } else if (proc.env.contains(QStringLiteral("SteamGameId"))) {
        steamAppId = proc.env.value(QStringLiteral("SteamGameId"));
    } else if (proc.env.contains(QStringLiteral("STEAM_COMPAT_APP_ID"))) {
        steamAppId = proc.env.value(QStringLiteral("STEAM_COMPAT_APP_ID"));
    }

    // Check ancestors env if empty
    if (steamAppId.isEmpty()) {
        for (qint64 apid : ancestors) {
            auto ainfo = ProcessInspector::inspectProcess(apid, m_procRoot);
            if (ainfo.env.contains(QStringLiteral("SteamAppId"))) {
                steamAppId = ainfo.env.value(QStringLiteral("SteamAppId"));
                break;
            }
        }
    }

    // Check cmdline
    if (steamAppId.isEmpty()) {
        static const QRegularExpression appidRe(QStringLiteral("steam://rungameid/(\\d+)"));
        auto m = appidRe.match(proc.cmdline);
        if (m.hasMatch()) {
            steamAppId = m.captured(1);
        }
    }

    // Check cwd path for steamapps/compatdata/
    if (steamAppId.isEmpty() && !proc.cwd.isEmpty()) {
        static const QRegularExpression compatRe(QStringLiteral("steamapps/compatdata/(\\d+)"));
        auto m = compatRe.match(proc.cwd);
        if (m.hasMatch()) {
            steamAppId = m.captured(1);
        }
    }

    // 2. Fetch Steam metadata if AppId found
    if (!steamAppId.isEmpty()) {
        auto steamApp = m_steamIndex->getAppInfo(steamAppId);
        if (steamApp) {
            result.displayName = steamApp->name;
            result.iconPath = m_steamIndex->findIconPath(steamAppId);
            result.iconName = QStringLiteral("steam_icon_") + steamAppId;
            result.source = QStringLiteral("steam-deep");
            result.showFallbackText = result.iconPath.isEmpty();
            return result;
        }
    }

    // 3. Wine executable resolution
    QString exeStem = WineExecutableResolver::parseExeStem(proc.cmdline, input.className);
    if (!exeStem.isEmpty()) {
        // Try matching with desktop files
        auto desktopSnap = m_desktopIndex->getEntries();
        for (const auto &entry : desktopSnap->entries) {
            if (entry.hidden || entry.noDisplay)
                continue;
            if (entry.id.contains(exeStem, Qt::CaseInsensitive) || entry.name.contains(exeStem, Qt::CaseInsensitive)) {
                result.iconName = entry.icon;
                result.displayName = entry.name;
                result.source = QStringLiteral("wine-desktop-match");
                return result;
            }
        }
        result.displayName = exeStem;
        result.iconName = QStringLiteral("application-x-executable");
        result.source = QStringLiteral("wine-fallback");
        return result;
    }

    // 4. Desktop entry matching (deep search)
    {
        AppIdentity deResult = resolveDesktopEntry(input);
        if (!deResult.iconName.isEmpty()) {
            result.iconName = deResult.iconName;
            result.displayName = deResult.displayName;
            result.source = QStringLiteral("desktop-deep");
            return result;
        }
    }

    // 5. Normal fallbacks (theme)
    {
        AppIdentity fbResult = resolveThemeFallback(input);
        result.iconName = fbResult.iconName;
        result.displayName = fbResult.displayName;
        result.source = fbResult.source;
        return result;
    }
}

void AppIdentityResolver::onDeepResolved(const WindowIdentityInput &input, const AppIdentity &identity, int generation) {
    if (static_cast<quint64>(generation) != input.openGeneration)
        return;
    if (identity.windowId != input.address || identity.pid != input.pid)
        return;
    if (identity.metadataFingerprint != input.metadataFingerprint)
        return;
    if (identity.themeRevision != m_themeRevision)
        return;
    if (identity.desktopIndexRevision != desktopIndexRevision())
        return;
    if (identity.steamIndexRevision != steamIndexRevision())
        return;

    const QString cacheKey = input.address + QLatin1Char(':') + QString::number(input.pid)
                             + QLatin1Char(':') + input.metadataFingerprint
                             + QLatin1Char(':') + QString::number(input.themeRevision)
                             + QLatin1Char(':') + QString::number(input.desktopIndexRevision)
                             + QLatin1Char(':') + QString::number(input.steamIndexRevision);
    m_cache.insert(cacheKey, identity);

    emit identityResolved(input.address, identity);
}

AppIdentity AppIdentityResolver::resolveAliases(const WindowIdentityInput &input) {
    AppIdentity result;
    const QString cls = input.className.toLower().trimmed();
    const QString initCls = input.initialClass.toLower().trimmed();
    const QString title = input.title.toLower().trimmed();

    if (cls == QStringLiteral("org.vinegarhq.sober") || initCls == QStringLiteral("org.vinegarhq.sober")) {
        result.iconName = QStringLiteral("org.vinegarhq.Sober");
        return result;
    }
    if (cls.contains(QStringLiteral("zen")) || initCls.contains(QStringLiteral("zen"))) {
        result.iconName = QStringLiteral("zen-browser");
        return result;
    }
    if (cls.contains(QStringLiteral("kitty")) || initCls.contains(QStringLiteral("kitty"))) {
        result.iconName = QStringLiteral("kitty");
        return result;
    }
    if (cls.contains(QStringLiteral("code")) || cls.contains(QStringLiteral("cursor")) ||
        initCls.contains(QStringLiteral("code")) || initCls.contains(QStringLiteral("cursor"))) {
        result.iconName = QStringLiteral("visual-studio-code");
        return result;
    }
    if (cls.contains(QStringLiteral("spotify")) || initCls.contains(QStringLiteral("spotify"))) {
        result.iconName = QStringLiteral("spotify");
        return result;
    }
    if (cls.contains(QStringLiteral("discord")) || initCls.contains(QStringLiteral("discord"))) {
        result.iconName = QStringLiteral("discord");
        return result;
    }
    if ((cls.contains(QStringLiteral("obs")) && !cls.contains(QStringLiteral("obsidian"))) ||
        cls == QStringLiteral("obsproject") || cls == QStringLiteral("obs-studio")) {
        result.iconName = QStringLiteral("com.obsproject.Studio");
        return result;
    }
    if (cls == QStringLiteral("obsidian") || initCls == QStringLiteral("obsidian")) {
        result.iconName = QStringLiteral("obsidian");
        return result;
    }
    return result;
}

AppIdentity AppIdentityResolver::resolveSteamAppId(const WindowIdentityInput &input) {
    AppIdentity result;
    const QString cls = input.className;
    if (cls == QStringLiteral("steam_app_default")) {
        result.iconPending = true;
        result.showFallbackText = false;
        return result;
    }

    static const QRegularExpression re(QStringLiteral("^steam_app_(\\d+)$"));
    const auto match = re.match(cls);
    if (match.hasMatch()) {
        const QString appId = match.captured(1);
        result.iconName = QStringLiteral("steam_icon_") + appId;
        result.iconPath = m_steamIndex->findIconPath(appId);
        if (!result.iconPath.isEmpty()) {
            result.showFallbackText = false;
        }
        // Use the window title as the display name for Steam games
        if (!input.title.isEmpty()) {
            result.displayName = input.title;
        }
        return result;
    }
    return result;
}

AppIdentity AppIdentityResolver::resolveDesktopEntry(const WindowIdentityInput &input) {
    AppIdentity result;
    const QString cls = input.className.toLower();
    const QString initCls = input.initialClass.toLower();
    const QString title = input.title.toLower();

    int bestScore = 0;
    QString bestIcon;
    QString bestName;

    auto desktopSnap = m_desktopIndex->getEntries();
    for (const auto &entry : desktopSnap->entries) {
        if (entry.hidden || entry.noDisplay)
            continue;
        int score = 0;
        const QString entryId = entry.id.toLower();
        const QString entryName = entry.name.toLower();
        const QString wmClass = entry.startupWmClass.toLower();

        if (!wmClass.isEmpty()) {
            if (wmClass == cls || wmClass == initCls) {
                score = 10;
            }
        }
        if (cls == entryId || initCls == entryId) {
            score = std::max(score, 8);
        }
        if (!entryName.isEmpty() && (cls.contains(entryName) || initCls.contains(entryName) ||
                                      (!cls.isEmpty() && entryName.contains(cls)) ||
                                      (!initCls.isEmpty() && entryName.contains(initCls)))) {
            score = std::max(score, 5);
        }
        if (input.className == QStringLiteral("org.quickshell") && !title.isEmpty() && entryName == title) {
            score = std::max(score, 7);
        }

        if (score > bestScore) {
            bestScore = score;
            bestIcon = entry.icon;
            bestName = entry.name;
        }
    }

    if (bestScore >= 5 && !bestIcon.isEmpty()) {
        result.iconName = bestIcon;
        result.displayName = bestName;
    }
    return result;
}

AppIdentity AppIdentityResolver::resolveThemeFallback(const WindowIdentityInput &input) {
    AppIdentity result;
    const QString cls = input.className;

    if (cls == QStringLiteral("org.quickshell")) {
        result.iconName = QStringLiteral("application-x-executable");
        return result;
    }

    if (cls.contains(QStringLiteral("steam"), Qt::CaseInsensitive) && !cls.contains(QStringLiteral("steam_app_"))) {
        result.iconName = QStringLiteral("steam");
        return result;
    }

    if (!cls.isEmpty()) {
        result.iconName = cls.toLower();
        return result;
    }

    if (!input.title.isEmpty()) {
        result.iconName = input.title.toLower().replace(QLatin1Char(' '), QLatin1Char('-'));
        return result;
    }
    return result;
}
