#include "SddmAutologin.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <stdexcept>
namespace
{
[[noreturn]] void fail(const QString &message)
{
    throw std::runtime_error(message.toUtf8().constData());
}
void merge(const QString &path, QVariantMap &values)
{
    if (!QFileInfo::exists(path))
        return;
    if (!QFileInfo(path).isReadable())
        fail("Cannot read SDDM configuration: " + path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
        fail("Cannot read SDDM configuration: " + path);
    bool active = false;
    for (const auto &raw : QString::fromUtf8(file.readAll()).split('\n')) {
        const auto line = raw.trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';'))
            continue;
        if (line.startsWith('[')) {
            active = line == "[Autologin]";
            continue;
        }
        if (!active)
            continue;
        const auto separator = line.indexOf('=');
        if (separator < 1)
            fail("Invalid Autologin entry in " + path);
        const auto key = line.left(separator).trimmed();
        if (!QStringList{"User", "Session", "Relogin"}.contains(key))
            continue;
        auto value = line.mid(separator + 1).trimmed();
        if (value.size() >= 2 && ((value.startsWith('"') && value.endsWith('"')) ||
                                  (value.startsWith('\'') && value.endsWith('\''))))
            value = value.mid(1, value.size() - 2);
        values[key] = value;
    }
}
} // namespace
namespace SddmAutologin
{
QVariantMap read(const QString &etcRoot, const QString &vendorRoot)
{
    QVariantMap result;
    for (const auto &directory : QStringList{vendorRoot, etcRoot + "/sddm.conf.d"})
        if (!directory.isEmpty())
            for (const auto &name : QDir(directory).entryList({"*.conf"}, QDir::Files, QDir::Name))
                merge(directory + "/" + name, result);
    // SDDM loads the main file last, overriding all drop-ins.
    merge(etcRoot + "/sddm.conf", result);
    return result;
}
void write(const QString &user, bool enabled, const QString &session, const QString &etcRoot)
{
    if (!QRegularExpression("^[A-Za-z0-9_][A-Za-z0-9_.-]{0,63}$").match(user).hasMatch())
        fail("Invalid account name");
    if (enabled &&
        !QRegularExpression("^[A-Za-z0-9_][A-Za-z0-9_.@+-]*\\.desktop$").match(session).hasMatch())
        fail("Invalid session name");
    const auto dir = etcRoot + "/sddm.conf.d";
    if (!QDir().mkpath(dir))
        fail("Cannot create SDDM configuration directory");
    const auto path = dir + "/zzzz-astrea-autologin.conf";
    QLockFile lock(path + ".lock");
    if (!lock.tryLock(5000))
        fail("SDDM configuration is locked");
    QVariantMap main;
    merge(etcRoot + "/sddm.conf", main);
    if (main.contains("User") || main.contains("Session"))
        fail("The main sddm.conf overrides autologin. Remove its Autologin/User override before "
             "using this control.");
    const auto current =
        read(etcRoot, etcRoot == "/etc" ? QString("/usr/lib/sddm/sddm.conf.d") : QString());
    const auto other = current.value("User").toString();
    if (!other.isEmpty() && other != user)
        fail("Automatic login is configured for a different user");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        fail(file.errorString());
    const auto bytes = (QString("[Autologin]\nUser=%1\nSession=%2\nRelogin=false\n")
                            .arg(enabled ? user : QString(), enabled ? session : QString()))
                           .toUtf8();
    if (file.write(bytes) != bytes.size() || !file.commit())
        fail(file.errorString());
}
} // namespace SddmAutologin
