#pragma once

#include "core/navigation/SettingsNavigationModel.hpp"
#include "services/assets/SettingsIconResolver.hpp"
#include "services/profile/SettingsUserProfile.hpp"

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

class SettingsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(SettingsNavigationModel *navigationModel READ navigationModel CONSTANT)
    Q_PROPERTY(QString selectedSectionId READ selectedSectionId NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedSectionTitle READ selectedSectionTitle NOTIFY selectionChanged)
    Q_PROPERTY(QUrl selectedPageSource READ selectedPageSource NOTIFY selectionChanged)
    Q_PROPERTY(QString filterText READ filterText NOTIFY filterTextChanged)
    Q_PROPERTY(QString userName READ userName CONSTANT)
    Q_PROPERTY(QUrl avatarUrl READ avatarUrl CONSTANT)
    Q_PROPERTY(bool isSudo READ isSudo CONSTANT)

public:
    explicit SettingsController(QObject *parent = nullptr);
    explicit SettingsController(SettingsUserProfile userProfile, QObject *parent = nullptr);
    SettingsController(std::unique_ptr<SettingsNavigationModel> navigationModel,
                       SettingsUserProfile userProfile,
                       SettingsIconResolver iconResolver,
                       QObject *parent = nullptr);

    SettingsNavigationModel *navigationModel();
    QString selectedSectionId() const;
    QString selectedSectionTitle() const;
    QUrl selectedPageSource() const;
    QString filterText() const;
    QString userName() const;
    QUrl avatarUrl() const;
    bool isSudo() const;

    Q_INVOKABLE bool selectSection(const QString &id);
    Q_INVOKABLE QUrl iconUrl(const QString &iconKey, const QString &iconTheme) const;
    Q_INVOKABLE void setFilterText(const QString &filterText);
    Q_INVOKABLE void clearFilter();

signals:
    void selectionChanged();
    void filterTextChanged();

private:
    std::unique_ptr<SettingsNavigationModel> m_navigationModel;
    const SettingsUserProfile m_userProfile;
    const SettingsIconResolver m_iconResolver;
};
