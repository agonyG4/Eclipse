#include "services/desktop/SddmAutologin.hpp"
#include <QCoreApplication>
#include <QFileInfo>
#include <QTextStream>
#include <limits>
#include <pwd.h>
#include <stdexcept>
#include <unistd.h>
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        bool ok = false;
        const auto raw = qEnvironmentVariable("PKEXEC_UID").toULongLong(&ok);
        if (geteuid() != 0 || !ok || raw == 0 || raw > std::numeric_limits<uid_t>::max())
            throw std::runtime_error("This helper requires an authenticated pkexec invocation");
        const auto args = app.arguments();
        if (args.size() != 4 || (args[1] != "enable" && args[1] != "disable") ||
            args[2] != "--session")
            throw std::runtime_error("Expected enable|disable --session SESSION.desktop");
        const auto *account = getpwuid(static_cast<uid_t>(raw));
        if (!account || !account->pw_name)
            throw std::runtime_error("Could not resolve the authenticated user");
        const QString user = QString::fromLocal8Bit(account->pw_name);
        const bool enabled = args[1] == "enable";
        if (enabled && (QFileInfo(args[3]).fileName() != args[3] ||
                        (!QFileInfo::exists("/usr/share/wayland-sessions/" + args[3]) &&
                         !QFileInfo::exists("/usr/share/xsessions/" + args[3]) &&
                         !QFileInfo::exists("/usr/local/share/wayland-sessions/" + args[3]) &&
                         !QFileInfo::exists("/usr/local/share/xsessions/" + args[3]))))
            throw std::runtime_error("The selected session is not installed");
        SddmAutologin::write(user, enabled, args[3]);
        return 0;
    } catch (const std::exception &error) {
        QTextStream(stderr) << error.what() << '\n';
        return 1;
    }
}
