#include "DesktopControllers.hpp"
#include "ProfileBackend.hpp"
#include "RegionSettings.hpp"
#include <QDesktopServices>
#include <QProcess>
#include <QStandardPaths>
namespace
{
QVariantMap regionSnapshot(const QString &path)
{
    auto state = RegionSettings::snapshot(path);
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
QVariantMap appsSnapshot(const ApplicationsBackend &backend, const QString &id = {})
{
    QVariantMap state{{"apps", backend.inventory()}};
    if (!id.isEmpty())
        state["selected"] = backend.details(id);
    return state;
}
} // namespace
SettingsRegionController::SettingsRegionController(QObject *parent)
    : SettingsRegionController(NativeSettings::configPath(), parent)
{
}
SettingsRegionController::SettingsRegionController(QString path, QObject *parent)
    : SettingsAsyncController(parent), m_path(std::move(path))
{
}
void SettingsRegionController::refresh()
{
    const auto path = m_path;
    submit([path] { return regionSnapshot(path); });
}
void SettingsRegionController::setOption(const QString &key, const QVariant &value)
{
    const auto path = m_path;
    submit(
        [path, key, value] {
            // Validate before performing any system action or creating a document.
            const auto current = RegionSettings::read(path);
            bool locationChanged = false;
            if (key == "automatic_location") {
                if (value.metaType().id() != QMetaType::Bool)
                    NativeSettings::fail("automatic_location must be a boolean");
                if (current["region"].toMap()[key].toBool() != value.toBool()) {
                    const auto status = regionSnapshot(path);
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
            return regionSnapshot(path);
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
