#include "core/SettingsController.hpp"

#include <QFileInfo>

SettingsController::SettingsController(QObject *parent)
    : QObject(parent)
    , m_navigationModel(this)
    , m_userName(resolveUserName())
    , m_avatarUrl(resolveAvatarUrl(m_userName))
{
    connect(&m_navigationModel, &SettingsNavigationModel::selectedIdChanged,
            this, &SettingsController::selectionChanged);
    connect(&m_navigationModel, &SettingsNavigationModel::filterTextChanged,
            this, &SettingsController::filterTextChanged);
}

SettingsNavigationModel *SettingsController::navigationModel()
{
    return &m_navigationModel;
}

QString SettingsController::selectedSectionId() const
{
    return m_navigationModel.selectedId();
}

QString SettingsController::selectedSectionTitle() const
{
    return m_navigationModel.titleForId(m_navigationModel.selectedId());
}

QString SettingsController::filterText() const
{
    return m_navigationModel.filterText();
}

QString SettingsController::userName() const
{
    return m_userName;
}

QUrl SettingsController::avatarUrl() const
{
    return m_avatarUrl;
}

bool SettingsController::pagesAvailable() const
{
    return false;
}

bool SettingsController::selectSection(const QString &id)
{
    return m_navigationModel.setSelectedId(id);
}

void SettingsController::setFilterText(const QString &filterText)
{
    m_navigationModel.setFilterText(filterText);
}

void SettingsController::clearFilter()
{
    m_navigationModel.setFilterText(QString());
}

QString SettingsController::resolveUserName()
{
    QString userName = qEnvironmentVariable("USER").trimmed();
    if (userName.isEmpty())
        userName = qEnvironmentVariable("LOGNAME").trimmed();
    if (userName.isEmpty())
        userName = QStringLiteral("User");
    return userName;
}

QUrl SettingsController::resolveAvatarUrl(const QString &userName)
{
    if (userName.isEmpty())
        return {};

    const QString path = QStringLiteral("/var/lib/AccountsService/icons/%1").arg(userName);
    const QFileInfo avatar(path);
    return avatar.isReadable() && avatar.isFile() ? QUrl::fromLocalFile(path) : QUrl();
}
