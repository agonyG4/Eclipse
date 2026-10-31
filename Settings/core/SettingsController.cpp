#include "services/devices/SettingsDeviceInventory.hpp"
#include "core/SettingsController.hpp"

#include "services/dock/SettingsDockController.hpp"
#include "services/wallpaper/SettingsWallpaperController.hpp"
#include "theme/ThemePackCatalog.hpp"
#include "theme/IconOverrides.hpp"
#include "launch/SessionInstance.hpp"

#include <utility>
#include <QLocalSocket>
#include <QGuiApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QRegularExpression>
#include <QColor>
#include <QStandardPaths>

namespace {
QString navigationIconsConfigPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/AstreaOS/ui/navigation-icons.json");
}
bool isValidNavigationIconWeight(const QString &weight)
{
    return weight == QStringLiteral("ultralight") || weight == QStringLiteral("thin")
        || weight == QStringLiteral("light") || weight == QStringLiteral("regular")
        || weight == QStringLiteral("medium") || weight == QStringLiteral("semibold")
        || weight == QStringLiteral("bold") || weight == QStringLiteral("heavy")
        || weight == QStringLiteral("black");
}
QString loadNavigationIconWeight()
{
    QFile file(navigationIconsConfigPath());
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("regular");
    const QString weight = QJsonDocument::fromJson(file.readAll()).object()
        .value(QStringLiteral("weight")).toString();
    return isValidNavigationIconWeight(weight) ? weight : QStringLiteral("regular");
}
QString loadVisualTheme()
{
    QFile file(navigationIconsConfigPath());
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("astrea");
    const QString requested = QJsonDocument::fromJson(file.readAll()).object()
        .value(QStringLiteral("visual_theme")).toString();
    return Astrea::ThemePacks::find(requested).has_value() ? requested
                                                           : QStringLiteral("astrea");
}
QString loadNavigationIconTheme()
{
    QFile file(navigationIconsConfigPath());
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("lucide");
    const QString requested = QJsonDocument::fromJson(file.readAll())
                                  .object().value(QStringLiteral("theme")).toString();
    return requested == QStringLiteral("apple") ? requested : QStringLiteral("lucide");
}
}

SettingsController::SettingsController(QObject *parent)
    : SettingsController(std::make_unique<SettingsNavigationModel>(), {}, {}, {}, {}, {}, parent)
{
}

SettingsController::SettingsController(SettingsUserProfile userProfile, QObject *parent)
    : SettingsController(std::make_unique<SettingsNavigationModel>(), std::move(userProfile),
                         {}, {}, {}, {}, parent)
{
}

SettingsController::SettingsController(std::unique_ptr<SettingsNavigationModel> navigationModel,
                                       SettingsUserProfile userProfile,
                                       SettingsIconResolver iconResolver,
                                       std::unique_ptr<Astrea::System::BluetoothService> bluetoothService,
                                       std::unique_ptr<Astrea::System::AudioService> audioService,
                                       std::unique_ptr<Astrea::System::NetworkService> networkService,
                                       QObject *parent)
    : QObject(parent)
    , m_navigationModel(std::move(navigationModel))
    , m_userProfile(std::move(userProfile))
    , m_iconResolver(std::move(iconResolver))
    , m_bluetoothService(bluetoothService ? std::move(bluetoothService)
                                          : std::make_unique<Astrea::System::BluetoothService>())
    , m_audioService(audioService ? std::move(audioService)
                                  : std::make_unique<Astrea::System::AudioService>())
    , m_networkService(networkService ? std::move(networkService)
                                      : std::make_unique<Astrea::System::NetworkService>())
    , m_firewallStatus(std::make_unique<SettingsFirewallStatus>(this))
    , m_firewallManager(std::make_unique<SettingsFirewallManager>(this))
    , m_spatialAudioController(std::make_unique<SettingsSpatialAudioController>())
{
    if (!m_navigationModel)
        m_navigationModel = std::make_unique<SettingsNavigationModel>();
    m_navigationModel->setParent(this);
    m_navigationIconTheme = loadNavigationIconTheme();
    m_navigationIconWeight = loadNavigationIconWeight();
    {
        QFile file(navigationIconsConfigPath());
        if (file.open(QIODevice::ReadOnly))
            m_navigationIconOverrides = QJsonDocument::fromJson(file.readAll())
                .object().value(QStringLiteral("overrides")).toObject();
    }
    m_visualTheme = loadVisualTheme();

    const QString preferredDestination = QStringLiteral("compositor");
    m_currentDestinationId = m_navigationModel->containsNavigableId(preferredDestination)
        ? preferredDestination
        : m_navigationModel->firstNavigableSidebarDestination();
    m_selectedSidebarId = m_navigationModel->sidebarAncestorForId(m_currentDestinationId);
    if (!m_currentDestinationId.isEmpty()) {
        m_history.append(m_currentDestinationId);
        m_historyIndex = 0;
    }

    m_wallpaperController = std::make_unique<SettingsWallpaperController>(QString(), this);
    m_dockController = std::make_unique<SettingsDockController>(QString(), this);
    m_dockPreviewUpdate.setSingleShot(true);
    m_dockPreviewUpdate.setInterval(16);
    connect(&m_dockPreviewUpdate, &QTimer::timeout, this,
            &SettingsController::publishDockPreviewState);
    connect(m_dockController.get(), &SettingsDockController::configChanged, this,
            [this] {
                if (!m_dockPreviewUpdate.isActive())
                    m_dockPreviewUpdate.start();
            });
    m_animationController = std::make_unique<SettingsAnimationController>();
    m_appearanceController = std::make_unique<SettingsAppearanceController>();
    m_iconsController = std::make_unique<SettingsIconsController>();
    m_visualEffectsController = std::make_unique<SettingsVisualEffectsController>();
    m_displayController = std::make_unique<SettingsDisplayController>();
    m_region = std::make_unique<SettingsRegionController>();
    m_applications = std::make_unique<SettingsApplicationsController>();
    m_profile = std::make_unique<SettingsProfileController>();
    connect(m_profile.get(), &SettingsProfileController::stateChanged, this, &SettingsController::userProfileChanged);
    m_gamingController = std::make_unique<SettingsGamingController>();
    m_performanceController = std::make_unique<SettingsPerformanceController>();
    m_screenshotConfiguration = std::make_unique<ScreenshotConfiguration>(QString(), this);
    m_screenshotPreviewUpdate.setSingleShot(true);
    m_screenshotPreviewUpdate.setInterval(16);
    connect(&m_screenshotPreviewUpdate, &QTimer::timeout, this,
            &SettingsController::publishScreenshotPreviewState);
    connect(m_screenshotConfiguration.get(), &ScreenshotConfiguration::configurationChanged,
            this, [this] {
                if (m_screenshotDemoActive && !m_screenshotPreviewUpdate.isActive())
                    m_screenshotPreviewUpdate.start();
            });
}

SettingsController::~SettingsController()
{
    syncDockOnExit();
}

void SettingsController::publishDockPreviewState()
{
    if (!m_dockController || QGuiApplication::platformName() == QStringLiteral("offscreen"))
        return;
    const QByteArray payload = m_dockController->livePreviewPayload().toBase64();
    if (payload.size() > 3000)
        return;
    QLocalSocket socket;
    socket.connectToServer(astrea::sessionScopedName(QStringLiteral("astrea-shell-v1")));
    if (!socket.waitForConnected(8))
        return;
    socket.write(QByteArrayLiteral("dock live ") + payload + '\n');
    socket.flush();
    socket.waitForBytesWritten(8);
    socket.disconnectFromServer();
}

void SettingsController::syncDockOnExit()
{
    m_dockPreviewUpdate.stop();
    if (!m_dockController || QGuiApplication::platformName() == QStringLiteral("offscreen"))
        return;
    m_dockController->flush();
    QLocalSocket socket;
    socket.connectToServer(astrea::sessionScopedName(QStringLiteral("astrea-shell-v1")));
    if (!socket.waitForConnected(50))
        return;
    socket.write(QByteArrayLiteral("dock sync\n"));
    socket.flush();
    socket.waitForBytesWritten(50);
    socket.disconnectFromServer();
}

SettingsNavigationModel *SettingsController::navigationModel()
{
    return m_navigationModel.get();
}

bool SettingsController::copyGamingLaunchCommand()
{
    const auto state = QJsonDocument::fromJson(
        m_gamingController->property("stateJson").toString().toUtf8()).object();
    const QString command = state.value(QStringLiteral("status")).toObject()
                                .value(QStringLiteral("launch_command")).toString();
    auto *clipboard = QGuiApplication::clipboard();
    if (command.isEmpty() || !clipboard)
        return false;
    clipboard->setText(command);
    return true;
}

QString SettingsController::currentDestinationId() const
{
    return m_currentDestinationId;
}

QString SettingsController::selectedSidebarId() const
{
    return m_selectedSidebarId;
}

QVariantMap SettingsController::currentDestination() const
{
    return m_navigationModel->descriptorForId(m_currentDestinationId);
}

QVariantList SettingsController::currentDestinationChildren() const
{
    return m_navigationModel->childDescriptorsForId(m_currentDestinationId);
}

QUrl SettingsController::selectedPageSource() const
{
    return m_navigationModel->pageSourceForId(m_currentDestinationId);
}

bool SettingsController::canGoBack() const
{
    return m_historyIndex > 0;
}

bool SettingsController::canGoForward() const
{
    return m_historyIndex >= 0 && m_historyIndex + 1 < m_history.size();
}

QString SettingsController::userName() const
{
    return m_profile->state().value("displayName", m_userProfile.userName).toString();
}

QUrl SettingsController::avatarUrl() const
{
    return m_profile->state().value("avatar", m_userProfile.avatarUrl).toUrl();
}

bool SettingsController::isSudo() const
{
    return m_userProfile.administrator;
}

bool SettingsController::navigateTo(const QString &id)
{
    if (!m_navigationModel->containsNavigableId(id))
        return false;
    if (id == m_currentDestinationId)
        return true;

    if (!setCurrentDestination(id))
        return false;
    appendHistory(id);
    emit navigationChanged();
    emit historyChanged();
    return true;
}

void SettingsController::goBack()
{
    if (!canGoBack())
        return;

    --m_historyIndex;
    setCurrentDestination(m_history.at(m_historyIndex));
    emit navigationChanged();
    emit historyChanged();
}

void SettingsController::goForward()
{
    if (!canGoForward())
        return;

    ++m_historyIndex;
    setCurrentDestination(m_history.at(m_historyIndex));
    emit navigationChanged();
    emit historyChanged();
}

QVariantList SettingsController::availableVisualThemes() const
{
    return Astrea::ThemePacks::visualThemes();
}

QVariantList SettingsController::availableNavigationIconPacks() const
{
    return Astrea::ThemePacks::list();
}

QUrl SettingsController::iconUrl(const QString &iconKey, const QString &iconTheme) const
{
    return iconUrlForWeight(iconKey, iconTheme, m_navigationIconWeight);
}

QUrl SettingsController::iconUrlForWeight(const QString &iconKey, const QString &iconTheme,
                                          const QString &weight) const
{
    if (iconTheme == m_navigationIconTheme) {
        const auto selected = Astrea::IconOverrides::resolve(
            m_navigationIconOverrides, QStringLiteral("settings"), iconKey,
            iconTheme, weight);
        if (selected.file.isValid())
            return selected.file;
        return m_iconResolver.resolve(selected.icon, selected.theme, selected.weight);
    }
    return m_iconResolver.resolve(iconKey, iconTheme, weight);
}

QUrl SettingsController::tintedSymbolUrl(const QString &iconKey, const QString &iconTheme,
                                          const QString &weight, const QString &hexColor) const
{
    return tintedSymbolSource(iconUrlForWeight(iconKey, iconTheme, weight), hexColor);
}

QUrl SettingsController::tintedSymbolSource(const QUrl &source, const QString &hexColor) const
{
    // Do not alter user-installed Apple SVGs. This is an in-memory presentation
    // copy so the SVG can be rasterized directly at the display's physical DPI.
    const QColor tint(hexColor);
    if (!tint.isValid() || !hexColor.startsWith(QLatin1Char('#')))
        return {};
    const QString path = source.isLocalFile()
        ? source.toLocalFile()
        : source.scheme() == QStringLiteral("qrc")
            ? QLatin1Char(':') + source.path() : QString{};
    if (path.isEmpty())
        return {};

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    if (file.size() > 131072)
        return source; // Preserve supported user-provided raster icon overrides.
    const QFileInfo iconFile(path);
    const QString cacheKey = path + QLatin1Char('|') + tint.name(QColor::HexRgb)
        + QLatin1Char('|') + QString::number(iconFile.size())
        + QLatin1Char('|') + QString::number(iconFile.lastModified().toMSecsSinceEpoch());
    if (const QUrl *cached = m_tintedSymbolCache.object(cacheKey))
        return *cached;
    QByteArray svg = file.readAll();
    if (!svg.contains("<svg") || !svg.contains("viewBox"))
        return source; // PNG and non-SVG icon overrides remain displayable.

    // Preserve the original SF symbol geometry and alpha coverage. Replacing
    // CSS currentColor avoids the 24px ColorOverlay scene-graph texture pass
    // which previously clipped and blurred small, detailed SVGs.
    const QByteArray tintHex = tint.name(QColor::HexRgb).toUtf8();
    svg.replace("currentColor", tintHex);
    // Apple reference SVGs may hardcode black/white rather than currentColor.
    // Preserve 'none', opacity, paths, and geometry (including cutouts).
    static const QRegularExpression monochromePaint(
        QStringLiteral(R"svg(((?:fill|stroke)\s*=\s*["'])(?:black|white|#[0-9a-fA-F]{3,8})(["']))svg"),
        QRegularExpression::CaseInsensitiveOption);
    QString recolored = QString::fromUtf8(svg);
    recolored.replace(monochromePaint, QStringLiteral("\\1") + QString::fromLatin1(tintHex)
                                   + QStringLiteral("\\2"));
    svg = recolored.toUtf8();

    // SF Symbols from the reference set use 1em root dimensions. QtSvg
    // interprets physical dimensions more reliably than relative em units.
    static const QRegularExpression viewBoxPattern(
        QStringLiteral(R"svg(viewBox="([-+\d.]+)\s+([-+\d.]+)\s+([\d.]+)\s+([\d.]+)")svg"));
    const auto match = viewBoxPattern.match(QString::fromUtf8(svg.left(1024)));
    if (match.hasMatch()) {
        bool widthOk = false;
        bool heightOk = false;
        const double width = match.captured(3).toDouble(&widthOk);
        const double height = match.captured(4).toDouble(&heightOk);
        if (widthOk && heightOk && width > 0.0 && height > 0.0) {
            // Small optical margin prevents paths touching the vector bounds.
            const double x = match.captured(1).toDouble() - width * 0.04;
            const double y = match.captured(2).toDouble() - height * 0.04;
            const double paddedWidth = width * 1.08;
            const double paddedHeight = height * 1.08;
            const QByteArray oldViewBox = match.captured(0).toUtf8();
            const QByteArray newViewBox = QStringLiteral("viewBox=\"%1 %2 %3 %4\"")
                .arg(x, 0, 'f', 4).arg(y, 0, 'f', 4)
                .arg(paddedWidth, 0, 'f', 4).arg(paddedHeight, 0, 'f', 4).toUtf8();
            svg.replace(oldViewBox, newViewBox);
            svg.replace("width=\"1em\"", QByteArray("width=\"") + QByteArray::number(paddedWidth) + "px\"");
            svg.replace("height=\"1em\"", QByteArray("height=\"") + QByteArray::number(paddedHeight) + "px\"");
            svg.replace("width='1em'", QByteArray("width='") + QByteArray::number(paddedWidth) + "px'");
            svg.replace("height='1em'", QByteArray("height='") + QByteArray::number(paddedHeight) + "px'");
        }
    }
    const QUrl result(QStringLiteral("data:image/svg+xml;base64,")
                      + QString::fromLatin1(svg.toBase64()));
    m_tintedSymbolCache.insert(cacheKey, new QUrl(result));
    return result;
}

bool SettingsController::setNavigationIconTheme(const QString &theme)
{
    if (theme != QStringLiteral("lucide") && theme != QStringLiteral("apple"))
        return false;
    if (theme == m_navigationIconTheme)
        return true;
    const QString path = navigationIconsConfigPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;

    QJsonObject settings;
    QFile previous(path);
    if (previous.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(previous.readAll());
        if (document.isObject())
            settings = document.object();
    }
    settings.insert(QStringLiteral("theme"), theme);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(settings).toJson(QJsonDocument::Indented)) < 0
        || !file.commit())
        return false;
    m_navigationIconTheme = theme;
    emit navigationIconThemeChanged();
    return true;
}

bool SettingsController::setNavigationIconWeight(const QString &weight)
{
    if (!isValidNavigationIconWeight(weight))
        return false;
    if (weight == m_navigationIconWeight)
        return true;

    const QString path = navigationIconsConfigPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QJsonObject settings;
    QFile previous(path);
    if (previous.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(previous.readAll());
        if (document.isObject())
            settings = document.object();
    }
    settings.insert(QStringLiteral("weight"), weight);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(settings).toJson(QJsonDocument::Indented)) < 0
        || !file.commit())
        return false;
    m_navigationIconWeight = weight;
    emit navigationIconWeightChanged();
    return true;
}

bool SettingsController::setVisualTheme(const QString &id)
{
    if (id != QStringLiteral("astrea") && !Astrea::ThemePacks::find(id).has_value())
        return false;
    if (id == m_visualTheme)
        return true;
    const QString path = navigationIconsConfigPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QJsonObject settings;
    QFile previous(path);
    if (previous.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(previous.readAll());
        if (document.isObject())
            settings = document.object();
    }
    settings.insert(QStringLiteral("visual_theme"), id);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(settings).toJson(QJsonDocument::Indented)) < 0
        || !file.commit())
        return false;
    m_visualTheme = id;
    emit visualThemeChanged();
    return true;
}

bool SettingsController::setCurrentDestination(const QString &id)
{
    if (!m_navigationModel->containsNavigableId(id))
        return false;
    const QString sidebarId = m_navigationModel->sidebarAncestorForId(id);
    if (sidebarId.isEmpty())
        return false;
    m_currentDestinationId = id;
    m_selectedSidebarId = sidebarId;
    return true;
}

void SettingsController::appendHistory(const QString &id)
{
    if (m_historyIndex + 1 < m_history.size())
        m_history.resize(m_historyIndex + 1);
    m_history.append(id);
    m_historyIndex = static_cast<int>(m_history.size()) - 1;

    if (m_history.size() > kMaximumHistoryEntries) {
        const int countToRemove = static_cast<int>(m_history.size()) - kMaximumHistoryEntries;
        m_history.remove(0, countToRemove);
        m_historyIndex -= countToRemove;
    }
}

void SettingsController::publishScreenshotPreviewState()
{
    if (!m_screenshotDemoActive || !m_screenshotConfiguration)
        return;
    const QByteArray payload = m_screenshotConfiguration->previewPayload().toBase64();
    QLocalSocket socket;
    socket.connectToServer(astrea::sessionScopedName(QStringLiteral("astrea-shell-v1")));
    if (!socket.waitForConnected(20))
        return;
    socket.write(QByteArrayLiteral("screenshot-config live ") + payload + '\n');
    socket.flush();
    socket.waitForBytesWritten(20);
    socket.disconnectFromServer();
}

bool SettingsController::screenshotDemoCommand(const QString &action)
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        return false;
    if (action != QStringLiteral("start") && action != QStringLiteral("stop")
        && action != QStringLiteral("replay") && action != QStringLiteral("renew"))
        return false;
    if (action == QStringLiteral("stop")) {
        m_screenshotDemoActive = false;
        m_screenshotPreviewUpdate.stop();
        m_screenshotConfiguration->flush();
    }
    QLocalSocket socket;
    socket.connectToServer(astrea::sessionScopedName(QStringLiteral("astrea-shell-v1")));
    if (!socket.waitForConnected(250))
        return false;
    const QByteArray command = QByteArray("screenshot-demo ") + action.toUtf8() + '\n';
    const bool sent = socket.write(command) == command.size() && socket.waitForBytesWritten(250);
    socket.disconnectFromServer();
    if (sent && action == QStringLiteral("start")) {
        m_screenshotDemoActive = true;
        m_screenshotPreviewUpdate.start();
    }
    return sent;
}

QVariantList SettingsController::connectedHardwareDevices() const
{
    return SettingsDeviceInventory::enumerate();
}
