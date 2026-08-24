#include "core/SettingsController.hpp"

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

    connect(m_navigationModel.get(), &SettingsNavigationModel::selectedIdChanged,
            this, &SettingsController::selectionChanged);
    connect(m_navigationModel.get(), &SettingsNavigationModel::filterTextChanged,
            this, &SettingsController::filterTextChanged);

    m_wallpaperController = std::make_unique<SettingsWallpaperController>(QString(), this);
}

SettingsNavigationModel *SettingsController::navigationModel()
{
    return m_navigationModel.get();
}

QString SettingsController::selectedSectionId() const
{
    return m_navigationModel->selectedId();
}

QString SettingsController::selectedSectionTitle() const
{
    return m_navigationModel->titleForId(m_navigationModel->selectedId());
}

QUrl SettingsController::selectedPageSource() const
{
    return m_navigationModel->pageSourceForId(m_navigationModel->selectedId());
}

QString SettingsController::filterText() const
{
    return m_navigationModel->filterText();
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

bool SettingsController::selectSection(const QString &id)
{
    return m_navigationModel->setSelectedId(id);
}

QUrl SettingsController::iconUrl(const QString &iconKey, const QString &iconTheme) const
{
    return m_iconResolver.resolve(iconKey, iconTheme);
}

void SettingsController::setFilterText(const QString &filterText)
{
    m_navigationModel->setFilterText(filterText);
}

void SettingsController::clearFilter()
{
    m_navigationModel->setFilterText(QString());
}
