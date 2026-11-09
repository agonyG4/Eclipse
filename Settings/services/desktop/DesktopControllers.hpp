#pragma once
#include "ApplicationsBackend.hpp"
#include "NativeSettingsSupport.hpp"
#include "RegionLocation.hpp"
#include <QUrl>
#include <atomic>
#include <memory>

// The system boundaries of the region page, replaceable in tests.
struct SettingsRegionServices {
    enum class ZoneChange { Applied, Denied, Failed };
    std::function<RegionLocation::Fix(const std::atomic_bool &)> locate;
    std::function<RegionLocation::Fix(const std::atomic_bool &)> lookupByIp;
    std::function<QString()> currentTimeZone;
    std::function<ZoneChange(const QString &)> setTimeZone;
    static SettingsRegionServices system();
};
class SettingsRegionController final : public SettingsAsyncController
{
    Q_OBJECT
    // status: idle, locating, found, blocked, unavailable, denied, timeout, failed,
    // network, refused. source: "system" or "ip". Held only for the session; coordinates
    // are reduced to a country and a zone and dropped on the worker.
    Q_PROPERTY(QVariantMap detection READ detection NOTIFY detectionChanged)
  public:
    explicit SettingsRegionController(QObject *parent = nullptr);
    explicit SettingsRegionController(QString path, QObject *parent = nullptr);
    SettingsRegionController(QString path, SettingsRegionServices services,
                             QObject *parent = nullptr);
    ~SettingsRegionController() override;
    QVariantMap detection() const { return m_detection; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setOption(const QString &key, const QVariant &value);
    Q_INVOKABLE void setTimeZone(const QString &id);
    Q_INVOKABLE void detectLocation();
    Q_INVOKABLE void detectLocationByIp();
    Q_INVOKABLE void cancelDetection();
    Q_INVOKABLE void applyDetection();
    Q_INVOKABLE void dismissDetection();
    Q_INVOKABLE QVariantList filterCountries(const QString &query) const;
    // Without a query the selected country's zones come first, marked `inCountry`.
    Q_INVOKABLE QVariantList filterTimeZones(const QString &query) const;
  signals:
    void detectionChanged();

  private:
    void startDetection(const QString &source,
                        std::function<RegionLocation::Fix(const std::atomic_bool &)> request);
    void setDetection(QVariantMap detection);
    QString m_path;
    SettingsRegionServices m_services;
    QVariantMap m_detection;
    std::shared_ptr<std::atomic_bool> m_cancel;
    QThreadPool m_detectionPool;
    QFutureWatcher<RegionLocation::Fix> m_detectionWatcher;
    QString m_detectionSource;
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
