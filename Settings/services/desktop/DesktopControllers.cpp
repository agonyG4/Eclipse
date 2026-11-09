#include "DesktopControllers.hpp"
#include "ProfileBackend.hpp"
#include "RegionCatalog.hpp"
#include "RegionSettings.hpp"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QProcess>
#include <QStandardPaths>
#include <QTimeZone>
#include <QtConcurrentRun>
namespace
{
QVariantMap regionSnapshot(const QString &path, const SettingsRegionServices &services)
{
    auto state = RegionSettings::snapshot(path);
    const auto zone = services.currentTimeZone();
    const auto language = state["config"].toMap()["language"].toString();
    const auto zones = RegionCatalog::timeZones(language, zone);
    state["timeZones"] = zones;
    state["currentTimeZone"] = zone;
    const auto systemctl = QStandardPaths::findExecutable("systemctl");
    bool available = false;
    if (!systemctl.isEmpty()) {
        try {
            const auto status = NativeSettings::run(
                systemctl, {"show", "--property=LoadState", "--value", "geoclue.service"}, 3000);
            available = status.code == 0 && (status.out == "loaded" || status.out == "masked");
        } catch (const std::exception &) {
        }
    }
    state["locationAvailable"] = available && !QStandardPaths::findExecutable("pkexec").isEmpty();
    return state;
}
SettingsRegionServices::ZoneChange setSystemTimeZone(const QString &id)
{
    auto call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.timedate1"), QStringLiteral("/org/freedesktop/timedate1"),
        QStringLiteral("org.freedesktop.timedate1"), QStringLiteral("SetTimezone"));
    call << id << true; // Interactive: polkit may ask for the administrator password.
    const auto reply = QDBusConnection::systemBus().call(call, QDBus::Block, 120000);
    if (reply.type() != QDBusMessage::ErrorMessage)
        return SettingsRegionServices::ZoneChange::Applied;
    return reply.errorName().contains(QStringLiteral("AccessDenied")) ||
                   reply.errorName().contains(QStringLiteral("AuthenticationFailed")) ||
                   reply.errorName().contains(QStringLiteral("InteractiveAuthorization"))
               ? SettingsRegionServices::ZoneChange::Denied
               : SettingsRegionServices::ZoneChange::Failed;
}
QString detectionStatus(RegionLocation::Status status)
{
    using RegionLocation::Status;
    switch (status) {
    case Status::Found: return QStringLiteral("found");
    case Status::Unavailable: return QStringLiteral("unavailable");
    case Status::Denied: return QStringLiteral("denied");
    case Status::TimedOut: return QStringLiteral("timeout");
    case Status::Failed: return QStringLiteral("failed");
    case Status::NetworkError: return QStringLiteral("network");
    case Status::Refused: return QStringLiteral("refused");
    case Status::Cancelled: return QStringLiteral("idle");
    }
    return QStringLiteral("failed");
}
QVariantMap appsSnapshot(const ApplicationsBackend &backend, const QString &id = {})
{
    QVariantMap state{{"apps", backend.inventory()}};
    if (!id.isEmpty())
        state["selected"] = backend.details(id);
    return state;
}
} // namespace
SettingsRegionServices SettingsRegionServices::system()
{
    return {[](const std::atomic_bool &cancel) { return RegionLocation::locateWithPortal(cancel); },
            [](const std::atomic_bool &cancel) { return RegionLocation::lookupByIp(cancel); },
            [] { return QString::fromLatin1(QTimeZone::systemTimeZoneId()); },
            setSystemTimeZone};
}
SettingsRegionController::SettingsRegionController(QObject *parent)
    : SettingsRegionController(NativeSettings::configPath(), parent)
{
}
SettingsRegionController::SettingsRegionController(QString path, QObject *parent)
    : SettingsRegionController(std::move(path), SettingsRegionServices::system(), parent)
{
}
SettingsRegionController::SettingsRegionController(QString path, SettingsRegionServices services,
                                                   QObject *parent)
    : SettingsAsyncController(parent), m_path(std::move(path)), m_services(std::move(services)),
      m_detection{{"status", "idle"}}
{
    connect(&m_detectionWatcher, &QFutureWatcher<RegionLocation::Fix>::finished, this, [this] {
        const auto fix = m_detectionWatcher.result();
        QVariantMap detection{{"status", detectionStatus(fix.status)}, {"source", m_detectionSource}};
        if (fix.status == RegionLocation::Status::Found) {
            const auto language = state()["config"].toMap()["language"].toString();
            detection["country"] = fix.country;
            detection["countryName"] = RegionCatalog::countryName(fix.country, language);
            detection["zone"] = fix.zone;
            const auto zones = state()["timeZones"].toList();
            for (const auto &item : zones) {
                const auto zone = item.toMap();
                if (zone["id"].toString() == fix.zone) {
                    detection["zoneName"] = zone["name"];
                    detection["zoneOffset"] = zone["trailing"];
                    break;
                }
            }
        }
        setDetection(detection);
    });
}
SettingsRegionController::~SettingsRegionController()
{
    if (m_cancel)
        m_cancel->store(true);
    m_detectionPool.waitForDone();
}
void SettingsRegionController::refresh()
{
    const auto path = m_path;
    const auto services = m_services;
    submit([path, services] { return regionSnapshot(path, services); });
}
void SettingsRegionController::setDetection(QVariantMap detection)
{
    m_detection = std::move(detection);
    emit detectionChanged();
}
void SettingsRegionController::startDetection(
    const QString &source, std::function<RegionLocation::Fix(const std::atomic_bool &)> request)
{
    if (m_detectionWatcher.isRunning())
        return;
    m_detectionSource = source;
    m_cancel = std::make_shared<std::atomic_bool>(false);
    setDetection({{"status", "locating"}, {"source", source}});
    m_detectionWatcher.setFuture(
        QtConcurrent::run(&m_detectionPool, [request = std::move(request), cancel = m_cancel] {
            auto fix = request(*cancel);
            if (fix.status != RegionLocation::Status::Found)
                return fix;
            // Coordinates stop here: only the reference zone leaves the worker.
            if (fix.zone.isEmpty()) {
                const auto match = RegionCatalog::nearestZone(fix.latitude, fix.longitude);
                fix.country = match.country;
                fix.zone = match.zone;
            }
            fix.latitude = fix.longitude = 0;
            if (!RegionCatalog::isCountryCode(fix.country) || !RegionCatalog::isTimeZoneId(fix.zone))
                fix.status = RegionLocation::Status::Failed;
            return fix;
        }));
}
void SettingsRegionController::detectLocation()
{
    const auto region = state()["config"].toMap()["region"].toMap();
    if (region.contains("automatic_location") && !region["automatic_location"].toBool()) {
        setDetection({{"status", "blocked"}, {"source", "system"}});
        return;
    }
    startDetection(QStringLiteral("system"), m_services.locate);
}
void SettingsRegionController::detectLocationByIp()
{
    startDetection(QStringLiteral("ip"), m_services.lookupByIp);
}
void SettingsRegionController::cancelDetection()
{
    if (m_cancel)
        m_cancel->store(true);
}
void SettingsRegionController::dismissDetection()
{
    if (!m_detectionWatcher.isRunning())
        setDetection({{"status", "idle"}});
}
void SettingsRegionController::applyDetection()
{
    if (m_detection["status"] != "found" || busy())
        return;
    const auto country = m_detection["country"].toString();
    const auto zone = m_detection["zone"].toString();
    setDetection({{"status", "idle"}});
    const auto path = m_path;
    const auto services = m_services;
    submit(
        [path, services, country, zone] {
            RegionSettings::update(path, "country_code", country);
            auto outcome = SettingsRegionServices::ZoneChange::Applied;
            if (services.currentTimeZone() != zone)
                outcome = services.setTimeZone(zone);
            auto state = regionSnapshot(path, services);
            state["timeZoneChange"] = outcome == SettingsRegionServices::ZoneChange::Applied ? "applied"
                                      : outcome == SettingsRegionServices::ZoneChange::Denied ? "denied"
                                                                                              : "failed";
            return state;
        },
        true);
}
void SettingsRegionController::setTimeZone(const QString &id)
{
    const auto path = m_path;
    const auto services = m_services;
    submit(
        [path, services, id] {
            if (!RegionCatalog::isTimeZoneId(id))
                NativeSettings::fail("Unsupported time zone");
            const auto outcome = services.setTimeZone(id);
            auto state = regionSnapshot(path, services);
            state["timeZoneChange"] = outcome == SettingsRegionServices::ZoneChange::Applied ? "applied"
                                      : outcome == SettingsRegionServices::ZoneChange::Denied ? "denied"
                                                                                              : "failed";
            return state;
        },
        true);
}
QVariantList SettingsRegionController::filterCountries(const QString &query) const
{
    const auto countries = state()["countries"].toList();
    // Search results are ranked, so only the full list is grouped by letter.
    return query.trimmed().isEmpty() ? RegionCatalog::withLetterHeadings(countries)
                                     : RegionCatalog::filter(countries, query);
}
QVariantList SettingsRegionController::filterTimeZones(const QString &query) const
{
    const auto zones = state()["timeZones"].toList();
    if (!query.trimmed().isEmpty())
        return RegionCatalog::filter(zones, query);
    const auto country = state()["config"].toMap()["region"].toMap()["country_code"];
    QVariantList local, others;
    for (const auto &item : zones) {
        auto zone = item.toMap();
        const auto inCountry = zone["country"] == country;
        zone["inCountry"] = inCountry;
        (inCountry ? local : others).append(zone);
    }
    return local + others;
}
void SettingsRegionController::setOption(const QString &key, const QVariant &value)
{
    const auto path = m_path;
    const auto services = m_services;
    submit(
        [path, services, key, value] {
            // Validate before performing any system action or creating a document.
            const auto current = RegionSettings::read(path);
            bool locationChanged = false;
            if (key == "automatic_location") {
                if (value.metaType().id() != QMetaType::Bool)
                    NativeSettings::fail("automatic_location must be a boolean");
                if (current["region"].toMap()[key].toBool() != value.toBool()) {
                    const auto status = regionSnapshot(path, services);
                    if (!status["locationAvailable"].toBool())
                        NativeSettings::fail("GeoClue or system authentication is unavailable");
                    QStringList args{QStandardPaths::findExecutable("systemctl"),
                                     value.toBool() ? "unmask" : "mask"};
                    if (!value.toBool())
                        args.append("--now");
                    args.append("geoclue.service");
                    NativeSettings::require(QStandardPaths::findExecutable("pkexec"), args, 120000);
                    locationChanged = true;
                }
            }
            try {
                RegionSettings::update(path, key, value);
            } catch (const std::exception &error) {
                if (locationChanged)
                    NativeSettings::fail(
                        QString(
                            "System geolocation was changed, but saving its preference failed: ") +
                        QString::fromUtf8(error.what()));
                throw;
            }
            return regionSnapshot(path, services);
        },
        true);
}
SettingsApplicationsController::SettingsApplicationsController(QObject *parent)
    : SettingsAsyncController(parent), m_backend(ApplicationsBackend::system())
{
    connect(this, &SettingsAsyncController::stateChanged, this, [this] {
        const auto location = state().value("openLocation").toString();
        if (!location.isEmpty())
            QMetaObject::invokeMethod(
                this,
                [this, location] {
                    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(location)))
                        report("Could not open the file manager");
                },
                Qt::QueuedConnection);
    });
}
void SettingsApplicationsController::refresh()
{
    const auto backend = m_backend;
    submit([backend] { return appsSnapshot(backend); });
}
void SettingsApplicationsController::select(const QString &id)
{
    const auto backend = m_backend;
    submit([backend, id] { return appsSnapshot(backend, id); });
}
void SettingsApplicationsController::createShortcut(const QString &id)
{
    const auto backend = m_backend;
    submit(
        [backend, id] {
            backend.shortcut(id);
            return appsSnapshot(backend, id);
        },
        true);
}
void SettingsApplicationsController::openLocation(const QString &id)
{
    const auto backend = m_backend;
    submit([backend, id] {
        auto state = appsSnapshot(backend, id);
        state["openLocation"] = backend.location(id);
        return state;
    });
}

void SettingsApplicationsController::uninstall(const QString &id, const QString &expectedTarget)
{
    const auto backend = m_backend;
    submit(
        [backend, id, expectedTarget] {
            backend.uninstall(id, expectedTarget);
            return appsSnapshot(backend);
        },
        true);
}
void SettingsProfileController::refresh()
{
    submit([] { return ProfileBackend::snapshot(); });
}
void SettingsProfileController::setDisplayName(const QString &name)
{
    submit(
        [name] {
            ProfileBackend::setName(name);
            return ProfileBackend::snapshot();
        },
        true);
}
void SettingsProfileController::setAvatar(const QUrl &file)
{
    submit(
        [file] {
            ProfileBackend::setAvatar(file);
            return ProfileBackend::snapshot();
        },
        true);
}
void SettingsProfileController::setAutomaticLogin(bool enabled)
{
    submit(
        [enabled] {
            ProfileBackend::setAutomaticLogin(enabled);
            return ProfileBackend::snapshot();
        },
        true);
}
void SettingsProfileController::changePassword()
{
    submit([] {
        const auto terminal = QStandardPaths::findExecutable("xdg-terminal-exec");
        if (terminal.isEmpty() || !QProcess::startDetached(terminal, {"/usr/bin/passwd"}))
            NativeSettings::fail("Install xdg-terminal-exec to change the password in a terminal");
        return ProfileBackend::snapshot();
    });
}
