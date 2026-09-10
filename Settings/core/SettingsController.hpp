#pragma once

#include "core/navigation/SettingsNavigationModel.hpp"
#include "services/assets/SettingsIconResolver.hpp"
#include "services/dock/SettingsDockController.hpp"
#include "services/animation/SettingsAnimationController.hpp"
#include "services/profile/SettingsUserProfile.hpp"
#include "services/wallpaper/SettingsWallpaperController.hpp"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <memory>

class SettingsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(SettingsNavigationModel *navigationModel READ navigationModel CONSTANT)
    Q_PROPERTY(QString currentDestinationId READ currentDestinationId NOTIFY navigationChanged)
    Q_PROPERTY(QString selectedSidebarId READ selectedSidebarId NOTIFY navigationChanged)
    Q_PROPERTY(QVariantMap currentDestination READ currentDestination NOTIFY navigationChanged)
    Q_PROPERTY(QVariantList currentDestinationChildren READ currentDestinationChildren NOTIFY navigationChanged)
    Q_PROPERTY(QUrl selectedPageSource READ selectedPageSource NOTIFY navigationChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)
    Q_PROPERTY(QString userName READ userName CONSTANT)
    Q_PROPERTY(QUrl avatarUrl READ avatarUrl CONSTANT)
    Q_PROPERTY(bool isSudo READ isSudo CONSTANT)
    Q_PROPERTY(SettingsWallpaperController *wallpaper READ wallpaper CONSTANT)
    Q_PROPERTY(SettingsDockController *dock READ dock CONSTANT)
    Q_PROPERTY(SettingsAnimationController *animations READ animations CONSTANT)

public:
    explicit SettingsController(QObject *parent = nullptr);
    explicit SettingsController(SettingsUserProfile userProfile, QObject *parent = nullptr);
    SettingsController(std::unique_ptr<SettingsNavigationModel> navigationModel,
                       SettingsUserProfile userProfile,
                       SettingsIconResolver iconResolver,
                       QObject *parent = nullptr);

    SettingsNavigationModel *navigationModel();
    QString currentDestinationId() const;
    QString selectedSidebarId() const;
    QVariantMap currentDestination() const;
    QVariantList currentDestinationChildren() const;
    QUrl selectedPageSource() const;
    bool canGoBack() const;
    bool canGoForward() const;
    QString userName() const;
    QUrl avatarUrl() const;
    bool isSudo() const;
    SettingsWallpaperController *wallpaper() const { return m_wallpaperController.get(); }
    SettingsDockController *dock() const { return m_dockController.get(); }
    SettingsAnimationController *animations() const { return m_animationController.get(); }

    Q_INVOKABLE bool navigateTo(const QString &id);
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    Q_INVOKABLE QUrl iconUrl(const QString &iconKey, const QString &iconTheme) const;

signals:
    void navigationChanged();
    void historyChanged();

private:
    static constexpr int kMaximumHistoryEntries = 64;

    bool setCurrentDestination(const QString &id);
    void appendHistory(const QString &id);

    std::unique_ptr<SettingsNavigationModel> m_navigationModel;
    const SettingsUserProfile m_userProfile;
    const SettingsIconResolver m_iconResolver;
    std::unique_ptr<SettingsWallpaperController> m_wallpaperController;
    std::unique_ptr<SettingsDockController> m_dockController;
    std::unique_ptr<SettingsAnimationController> m_animationController;
    QString m_currentDestinationId;
    QString m_selectedSidebarId;
    QVector<QString> m_history;
    int m_historyIndex = -1;
};
