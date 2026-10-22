#include "ApplicationsBackend.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <algorithm>

namespace
{
bool inside(const QString &path, const QString &root)
{
    const auto canonical = QFileInfo(path).canonicalFilePath();
    const auto base = QFileInfo(root).canonicalFilePath();
    return !base.isEmpty() && canonical.startsWith(base + "/");
}
QString field(const QString &fileName, const QString &key)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        NativeSettings::fail(file.errorString());
    bool active = false;
    for (const auto &line : QString::fromUtf8(file.read(1024 * 1024)).split('\n')) {
        if (line.startsWith('['))
            active = line.trimmed() == "[Desktop Entry]";
        if (active && line.startsWith(key + "="))
            return line.mid(key.size() + 1).trimmed();
    }
    return {};
}
QString localized(const QString &fallback, const QHash<QString, QString> &names)
{
    const auto locale = QLocale().name();
    return names.value(locale, names.value(locale.section('_', 0, 0), fallback));
}
bool token(const QString &value)
{
    return QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_.+@-]*$").match(value).hasMatch();
}
} // namespace
ApplicationsBackend::ApplicationsBackend(QString dataHome, QStringList dataDirs, QString desktop,
                                         NativeSettings::Runner runner)
    : m_dataHome(std::move(dataHome)), m_dataDirs(std::move(dataDirs)),
      m_desktop(std::move(desktop)), m_runner(std::move(runner))
{
}
ApplicationsBackend ApplicationsBackend::system()
{
    auto roots = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    const auto home = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    roots.removeAll(home);
    roots.append(home + "/flatpak/exports/share");
    roots.append("/var/lib/flatpak/exports/share");
    return {home, roots, QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)};
}
QList<DesktopEntryRecord> ApplicationsBackend::records() const
{
    QList<DesktopEntryRecord> result;
    QSet<QString> seen;
    auto roots = m_dataDirs;
    roots.prepend(m_dataHome);
    for (const auto &root : roots) {
        const auto apps = QDir(root).filePath("applications");
        QDirIterator it(apps, {"*.desktop"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const auto path = it.next();
            if (QFileInfo(path).size() > 1024 * 1024)
                continue;
            auto id = QDir(apps).relativeFilePath(path);
            id.replace('/', '-');
            id.chop(8);
            if (seen.contains(id))
                continue;
            seen.insert(id);
            const auto entry = DesktopEntryParser::parse(path, apps);
            if (!entry || entry->hidden || entry->noDisplay)
                continue;
            if (!entry->tryExec.isEmpty() &&
                QStandardPaths::findExecutable(entry->tryExec).isEmpty())
                continue;
            result.append(*entry);
        }
    }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
    return result;
}
DesktopEntryRecord ApplicationsBackend::resolve(const QString &id) const
{
    for (const auto &record : records())
        if (record.id == id)
            return record;
    NativeSettings::fail("Application is no longer in the installed desktop catalog: " + id);
}
QVariantList ApplicationsBackend::inventory() const
{
    QVariantList list;
    for (const auto &record : records())
        list.append(QVariantMap{
            {"id", record.id},
            {"name", localized(record.name, record.localizedNames)},
            {"description", localized(record.comment, record.localizedComments)},
            {"category", record.categories.isEmpty() ? "Application" : record.categories[0]},
            {"file", record.sourceFilePath},
            {"icon", record.icon}});
    return list;
}
QVariantMap ApplicationsBackend::details(const QString &id) const
{
    const auto record = resolve(id);
    QVariantMap result{{"id", record.id},
                       {"name", localized(record.name, record.localizedNames)},
                       {"description", localized(record.comment, record.localizedComments)},
                       {"file", record.sourceFilePath},
                       {"kind", "Desktop application"},
                       {"size", "Not available"},
                       {"privacy", "Native application: permissions are not isolated per app."},
                       {"canUninstall", false},
                       {"target", ""},
                       {"actionLabel", "Uninstall"}};
    auto fp = field(record.sourceFilePath, "X-Flatpak");
    const auto flatpak = QStandardPaths::findExecutable("flatpak");
    if (token(fp) && !flatpak.isEmpty()) {
        const QString installation =
            inside(record.sourceFilePath, m_dataHome + "/flatpak") ? "--user" : "--system";
        const auto info = m_runner(flatpak, {"info", installation, "--show-size", "--", fp}, 10000);
        if (info.code == 0) {
            result["kind"] = "Flatpak";
            result["size"] = info.out;
            result["method"] = "flatpak";
            result["target"] = fp;
            result["installation"] = installation;
            result["canUninstall"] = true;
            const auto permissions =
                m_runner(flatpak, {"info", installation, "--show-permissions", "--", fp}, 10000);
            result["privacy"] = permissions.code == 0
                                    ? permissions.out
                                    : "Could not read Flatpak permissions: " + permissions.error;
        }
    } else if (inside(record.sourceFilePath, m_dataHome + "/applications")) {
        result["method"] = "launcher";
        result["target"] = record.sourceFilePath;
        result["canUninstall"] = true;
        result["actionLabel"] = "Remove launcher";
    } else {
        const auto pacman = QStandardPaths::findExecutable("pacman");
        if (!pacman.isEmpty()) {
            const auto owner = m_runner(pacman, {"-Qoq", "--", record.sourceFilePath}, 10000);
            const auto package = owner.out.trimmed();
            if (owner.code == 0 && token(package)) {
                result["method"] = "package";
                result["target"] = package;
                const QStringList protectedPackages = {
                    "eclipse", "astrea", "astrea-settings", "pacman",        "glibc", "systemd",
                    "sudo",    "polkit", "linux",           "linux-cachyos", "base"};
                result["canUninstall"] = !protectedPackages.contains(package) &&
                                         !QStandardPaths::findExecutable("pkexec").isEmpty();
                const auto info = m_runner(pacman, {"-Qi", "--", package}, 10000);
                const auto match =
                    QRegularExpression("Installed Size\\s*:\\s*([^\\n]+)").match(info.out);
                if (match.hasMatch())
                    result["size"] = match.captured(1).trimmed();
            }
        }
    }
    if (id.startsWith("astrea-settings") || record.exec.contains("steam://rungameid/"))
        result["canUninstall"] = false;
    const auto identity = QFileInfo(record.sourceFilePath).canonicalFilePath() + "\n" +
                          result.value("method").toString() + "\n" +
                          result.value("installation").toString() + "\n" +
                          result.value("target").toString();
    result["removalToken"] = QString::fromLatin1(
        QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex());
    return result;
}
QString ApplicationsBackend::location(const QString &id) const
{
    return QFileInfo(resolve(id).sourceFilePath).absolutePath();
}
void ApplicationsBackend::shortcut(const QString &id) const
{
    const auto record = resolve(id);
    if (m_desktop.isEmpty() || m_desktop == QDir::homePath())
        NativeSettings::fail("No desktop directory is configured");
    if (!QDir().mkpath(m_desktop))
        NativeSettings::fail("Cannot create desktop directory");
    const auto target = QDir(m_desktop).filePath(QFileInfo(record.sourceFilePath).fileName());
    // QFile::copy refuses existing files, including symlinks. Never overwrite a user's shortcut.
    if (!QFile::copy(record.sourceFilePath, target))
        NativeSettings::fail("Shortcut already exists or could not be created: " + target);
    if (!QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                           QFile::ReadGroup | QFile::ReadOther))
        NativeSettings::fail("Shortcut copied, but executable permission could not be set");
}
void ApplicationsBackend::uninstall(const QString &id, const QString &expectedTarget) const
{
    const auto current = details(id);
    if (!current["canUninstall"].toBool() || current["removalToken"].toString() != expectedTarget)
        NativeSettings::fail(
            "Application removal target changed or is protected. Refresh its details.");
    const auto method = current["method"].toString();
    const auto target = current["target"].toString();
    NativeSettings::CommandResult outcome;
    if (method == "launcher") {
        if (!inside(target, m_dataHome + "/applications") || !QFile::remove(target))
            NativeSettings::fail("Could not remove the user launcher");
        return;
    }
    if (method == "flatpak")
        outcome = m_runner(
            QStandardPaths::findExecutable("flatpak"),
            {"uninstall", current["installation"].toString(), "--assumeyes", "--app", "--", target},
            120000);
    else if (method == "package")
        outcome = m_runner(
            QStandardPaths::findExecutable("pkexec"),
            {QStandardPaths::findExecutable("pacman"), "-R", "--noconfirm", "--", target}, 120000);
    else
        NativeSettings::fail("Unsupported removal method");
    if (outcome.code != 0)
        NativeSettings::fail("Removal failed: " + outcome.error + " " + outcome.out);
}
