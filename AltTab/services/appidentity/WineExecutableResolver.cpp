#include "services/appidentity/WineExecutableResolver.hpp"
#include <QFileInfo>
#include <QRegularExpression>

QString WineExecutableResolver::parseExeStem(const QString &cmdline, const QString &className) {
    // 1. Try class name if it ends with .exe
    if (className.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        return QFileInfo(className).completeBaseName();
    }

    // 2. Scan cmdline for executable ending in .exe
    // We match any word or path that ends with .exe and is optionally quoted or preceded by space/slashes
    static const QRegularExpression re(QStringLiteral("(?:^|\\s|/|\\\\)([^\\s/\\\\]+\\.exe)(?:$|\\s|\\\"|\\')"), QRegularExpression::CaseInsensitiveOption);
    auto matches = re.globalMatch(cmdline);
    QString bestExe;
    while (matches.hasNext()) {
        auto m = matches.next();
        QString exe = m.captured(1);
        // Exclude generic wine/steam launchers
        if (exe.toLower() != QStringLiteral("wine.exe") &&
            exe.toLower() != QStringLiteral("wine64.exe") &&
            exe.toLower() != QStringLiteral("steam.exe")) {
            bestExe = exe;
        }
    }
    if (!bestExe.isEmpty()) {
        return QFileInfo(bestExe).completeBaseName();
    }

    return {};
}
