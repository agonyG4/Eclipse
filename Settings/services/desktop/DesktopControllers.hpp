#pragma once
#include "ApplicationsBackend.hpp"
#include "NativeSettingsSupport.hpp"
#include <QUrl>

class SettingsRegionController final : public SettingsAsyncController
{
    Q_OBJECT
  public:
    explicit SettingsRegionController(QObject *parent = nullptr);
    explicit SettingsRegionController(QString path, QObject *parent = nullptr);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setOption(const QString &key, const QVariant &value);

  private:
    QString m_path;
};
class SettingsApplicationsController final : public SettingsAsyncController
{
    Q_OBJECT
  public:
    explicit SettingsApplicationsController(QObject *parent = nullptr);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void select(const QString &id);
    Q_INVOKABLE void createShortcut(const QString &id);
    Q_INVOKABLE void openLocation(const QString &id);
    Q_INVOKABLE void uninstall(const QString &id, const QString &expectedTarget);

  private:
    ApplicationsBackend m_backend;
};
class SettingsProfileController final : public SettingsAsyncController
{
    Q_OBJECT
  public:
    using SettingsAsyncController::SettingsAsyncController;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setDisplayName(const QString &name);
    Q_INVOKABLE void setAvatar(const QUrl &file);
    Q_INVOKABLE void setAutomaticLogin(bool enabled);
    Q_INVOKABLE void changePassword();
};
