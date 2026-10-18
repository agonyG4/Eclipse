#include "core/SettingsController.hpp"

#include "services/dock/SettingsDockController.hpp"
#include "services/wallpaper/SettingsWallpaperController.hpp"
#include "theme/ThemePackCatalog.hpp"
#include "theme/IconOverrides.hpp"

#include <utility>
#include <QLocalSocket>
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
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
    socket.connectToServer(QStringLiteral("astrea-shell-v1"));
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
    socket.connectToServer(QStringLiteral("astrea-shell-v1"));
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
    return m_userProfile.userName;
}

QUrl SettingsController::avatarUrl() const
{
    return m_userProfile.avatarUrl;
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
    socket.connectToServer(QStringLiteral("astrea-shell-v1"));
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
    socket.connectToServer(QStringLiteral("astrea-shell-v1"));
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
