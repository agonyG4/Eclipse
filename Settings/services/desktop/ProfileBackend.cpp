#include "ProfileBackend.hpp"
#include "NativeSettingsSupport.hpp"
#include "SddmAutologin.hpp"
#include "platform/linux/AdminGroupDetector.hpp"
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QStandardPaths>
#include <pwd.h>
#include <unistd.h>
namespace
{
QVariantMap account()
{
    char buffer[16384];
    passwd entry{};
    passwd *found = nullptr;
    if (getpwuid_r(getuid(), &entry, buffer, sizeof(buffer), &found) != 0 || !found)
        NativeSettings::fail("Could not resolve the current user");
    return {{"user", QString::fromLocal8Bit(entry.pw_name)},
            {"displayName", QString::fromLocal8Bit(entry.pw_gecos).section(',', 0, 0)}};
}
QDBusMessage call(const QString &path, const QString &interface, const QString &method,
                  const QVariantList &args, bool mutation = false)
{
    auto request =
        QDBusMessage::createMethodCall("org.freedesktop.Accounts", path, interface, method);
    request.setArguments(args);
    request.setInteractiveAuthorizationAllowed(mutation);
    const auto reply =
        QDBusConnection::systemBus().call(request, QDBus::Block, mutation ? 120000 : 5000);
    if (reply.type() == QDBusMessage::ErrorMessage)
        NativeSettings::fail(reply.errorMessage());
    return reply;
}
QString userPath()
{
    const auto reply = call("/org/freedesktop/Accounts", "org.freedesktop.Accounts", "FindUserById",
                            {QVariant::fromValue(static_cast<qlonglong>(getuid()))});
    if (reply.arguments().size() != 1)
        NativeSettings::fail("Invalid AccountsService response");
    return qvariant_cast<QDBusObjectPath>(reply.arguments()[0]).path();
}
QString session()
{
    const auto configured = SddmAutologin::read().value("Session").toString();
    if (!configured.isEmpty() && QFileInfo(configured).fileName() == configured &&
        (QFileInfo::exists("/usr/share/wayland-sessions/" + configured) ||
         QFileInfo::exists("/usr/local/share/wayland-sessions/" + configured) ||
         QFileInfo::exists("/usr/share/xsessions/" + configured) ||
         QFileInfo::exists("/usr/local/share/xsessions/" + configured)))
        return configured;
    for (const auto &dir :
         QStringList{"/usr/local/share/wayland-sessions", "/usr/share/wayland-sessions",
                     "/usr/local/share/xsessions", "/usr/share/xsessions"}) {
        const auto entries = QDir(dir).entryList({"*.desktop"}, QDir::Files, QDir::Name);
        // Prefer the active session when it names an installed desktop file.
        const auto active = qEnvironmentVariable("XDG_SESSION_DESKTOP") + ".desktop";
        if (entries.contains(active))
            return active;
        if (!entries.isEmpty())
            return entries[0];
    }
    return {};
}
} // namespace
namespace ProfileBackend
{
QString validateName(const QString &name)
{
    for (const auto ch : name)
        if (ch.unicode() < 32 || ch.unicode() == 127)
            NativeSettings::fail("Display name contains control characters");
    const auto trimmed = name.simplified();
    if (trimmed.isEmpty() || trimmed.size() > 128)
        NativeSettings::fail("Display name must contain 1 to 128 characters");
    return trimmed;
}
QVariantMap snapshot()
{
    auto result = account();
    if (result["displayName"].toString().isEmpty())
        result["displayName"] = result["user"];
    result["administrator"] = AdminGroupDetector().isCurrentUserAdministrator();
    result["avatarVersion"] = static_cast<int>(QDateTime::currentMSecsSinceEpoch() % 2147483647);
    result["accountsAvailable"] = false;
    result["avatar"] = QString();
    try {
        const auto reply = call(userPath(), "org.freedesktop.DBus.Properties", "GetAll",
                                {"org.freedesktop.Accounts.User"});
        if (reply.arguments().size() != 1)
            NativeSettings::fail("Invalid account property response");
        const auto values = qdbus_cast<QVariantMap>(reply.arguments()[0]);
        if (!values["RealName"].toString().isEmpty())
            result["displayName"] = values["RealName"];
        const auto icon = values["IconFile"].toString();
        if (QFileInfo(icon).isReadable())
            result["avatar"] = QUrl::fromLocalFile(icon);
        result["accountsAvailable"] = true;
    } catch (const std::exception &error) {
        result["accountError"] = QString::fromUtf8(error.what());
    }
    const auto sddm = SddmAutologin::read();
    result["automaticLogin"] = sddm.value("User").toString() == result["user"].toString();
    result["session"] = session();
    result["autologinAvailable"] =
        QFileInfo::exists(QStringLiteral(ASTREA_PROFILE_HELPER_PATH)) &&
        (QFileInfo::exists("/usr/bin/sddm") || !QStandardPaths::findExecutable("sddm").isEmpty()) &&
        !QStandardPaths::findExecutable("pkexec").isEmpty() &&
        !result["session"].toString().isEmpty();
    result["passwordAvailable"] = !QStandardPaths::findExecutable("xdg-terminal-exec").isEmpty() &&
                                  QFileInfo::exists("/usr/bin/passwd");
    return result;
}
void setName(const QString &name)
{
    const auto normalized = validateName(name);
    call(userPath(), "org.freedesktop.Accounts.User", "SetRealName", {normalized}, true);
}
void setAvatar(const QUrl &url)
{
    if (!url.isLocalFile())
        NativeSettings::fail("Choose a local image");
    const QFileInfo file(url.toLocalFile());
    if (!file.isFile() || !file.isReadable() || file.size() > 10 * 1024 * 1024)
        NativeSettings::fail("Avatar must be a readable image smaller than 10 MiB");
    QImageReader reader(file.absoluteFilePath());
    const auto size = reader.size();
    if (!reader.canRead() || !size.isValid() || size.width() > 8192 || size.height() > 8192)
        NativeSettings::fail("Unsupported avatar image or dimensions");
    call(userPath(), "org.freedesktop.Accounts.User", "SetIconFile", {file.canonicalFilePath()},
         true);
}
void setAutomaticLogin(bool enabled)
{
    const auto selected = session();
    if (selected.isEmpty())
        NativeSettings::fail("No installed desktop session was found");
    NativeSettings::require(QStandardPaths::findExecutable("pkexec"),
                            {QStringLiteral(ASTREA_PROFILE_HELPER_PATH),
                             enabled ? "enable" : "disable", "--session", selected},
                            120000);
    const auto state = snapshot();
    if (state["automaticLogin"].toBool() != enabled)
        NativeSettings::fail("SDDM configuration still overrides this automatic-login setting");
}
} // namespace ProfileBackend
