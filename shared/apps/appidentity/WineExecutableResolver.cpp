#include "apps/appidentity/WineExecutableResolver.hpp"

#include <QFileInfo>
#include <QRegularExpression>

QString WineExecutableResolver::parseExeStem(const QString &cmdline, const QString &className) {
    if (className.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive))
        return QFileInfo(className).completeBaseName();

    static const QRegularExpression re(QStringLiteral("(?:^|\\s|/|\\\\)([^\\s/\\\\]+\\.exe)(?:$|\\s|\\\"|\\')"), QRegularExpression::CaseInsensitiveOption);
    auto matches = re.globalMatch(cmdline);
    QString bestExe;
    while (matches.hasNext()) {
        auto match = matches.next();
        QString exe = match.captured(1);
        if (exe.toLower() != QStringLiteral("wine.exe")
            && exe.toLower() != QStringLiteral("wine64.exe")
            && exe.toLower() != QStringLiteral("steam.exe")) {
            bestExe = exe;
        }
    }
    if (!bestExe.isEmpty())
        return QFileInfo(bestExe).completeBaseName();
    return {};
}
