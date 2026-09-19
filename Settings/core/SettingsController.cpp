#include "core/SettingsController.hpp"

#include "services/dock/SettingsDockController.hpp"
#include "services/wallpaper/SettingsWallpaperController.hpp"

#include <utility>

SettingsController::SettingsController(QObject *parent)
    : SettingsController(std::make_unique<SettingsNavigationModel>(), {}, {}, parent)
{
}

SettingsController::SettingsController(SettingsUserProfile userProfile, QObject *parent)
    : SettingsController(std::make_unique<SettingsNavigationModel>(), std::move(userProfile), {}, parent)
{
}

SettingsController::SettingsController(std::unique_ptr<SettingsNavigationModel> navigationModel,
                                       SettingsUserProfile userProfile,
                                       SettingsIconResolver iconResolver,
                                       QObject *parent)
    : QObject(parent)
    , m_navigationModel(std::move(navigationModel))
    , m_userProfile(std::move(userProfile))
    , m_iconResolver(std::move(iconResolver))
{
    if (!m_navigationModel)
        m_navigationModel = std::make_unique<SettingsNavigationModel>();
    m_navigationModel->setParent(this);

    m_currentDestinationId = m_navigationModel->firstNavigableSidebarDestination();
    m_selectedSidebarId = m_navigationModel->sidebarAncestorForId(m_currentDestinationId);
    if (!m_currentDestinationId.isEmpty()) {
        m_history.append(m_currentDestinationId);
        m_historyIndex = 0;
    }

    m_wallpaperController = std::make_unique<SettingsWallpaperController>(QString(), this);
    m_dockController = std::make_unique<SettingsDockController>(QString(), this);
    m_animationController = std::make_unique<SettingsAnimationController>();
    m_themesController = std::make_unique<SettingsThemesController>();
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

QUrl SettingsController::iconUrl(const QString &iconKey, const QString &iconTheme) const
{
    return m_iconResolver.resolve(iconKey, iconTheme);
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
