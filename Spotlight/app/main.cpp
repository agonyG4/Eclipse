#include "app/CommandLine.hpp"
#include "icons/AstreaIconProvider.hpp"
#include "icons/AstreaIconTheme.hpp"
#include "platform/ipc/ShellIpcClient.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace {

constexpr QLatin1StringView kShellIpcName("astrea-shell-v1");

int runIconDiagnostics(int argc, char **argv, const CommandLineRequest &request)
{
    QGuiApplication app(argc, argv);
    AstreaIconTheme::apply();
    if (request.mode == CommandLineRequest::Mode::IconThemeDiagnostics) {
        const auto resolved = AstreaIconTheme::resolveWithSource();
        QJsonArray searchPaths;
        for (const QString &path : AstreaIconTheme::searchPaths())
            searchPaths.append(path);
        const QJsonObject result{
            {QStringLiteral("selected"), resolved.theme},
            {QStringLiteral("source"), resolved.source},
            {QStringLiteral("qtTheme"), QIcon::themeName()},
            {QStringLiteral("fallback"), QIcon::fallbackThemeName()},
            {QStringLiteral("searchPaths"), searchPaths}
        };
        QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Indented) << '\n';
        return 0;
    }

    AstreaIconProvider provider;
    QSize size;
    const QPixmap pixmap = provider.requestPixmap(request.argument, &size, QSize(48, 48));
    const QJsonObject result{
        {QStringLiteral("iconName"), request.argument},
        {QStringLiteral("selectedTheme"), QIcon::themeName()},
        {QStringLiteral("fallbackTheme"), QIcon::fallbackThemeName()},
        {QStringLiteral("resolved"), !pixmap.isNull()},
        {QStringLiteral("requestedSize"), QStringLiteral("48x48")},
        {QStringLiteral("size"), pixmap.isNull()
            ? QString()
            : QStringLiteral("%1x%2").arg(size.width()).arg(size.height())}
    };
    QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Indented) << '\n';
    return pixmap.isNull() ? 1 : 0;
}

}

int main(int argc, char **argv)
{
    QStringList arguments;
    for (int index = 0; index < argc; ++index)
        arguments.append(QString::fromLocal8Bit(argv[index]));
    const CommandLineRequest request = CommandLine::parse(arguments.mid(1));
    if (request.mode == CommandLineRequest::Mode::IconThemeDiagnostics
        || request.mode == CommandLineRequest::Mode::ResolveIcon)
        return runIconDiagnostics(argc, argv, request);

    QCoreApplication app(argc, argv);
    if (request.daemonMode) {
        QTextStream(stderr) << "astrea-spotlight is a compatibility IPC client; use astrea-shell\n";
        return 2;
    }

    if (request.mode == CommandLineRequest::Mode::Status) {
        const QByteArray reply = ShellIpcClient::requestReply(QString::fromLatin1(kShellIpcName),
                                                              QStringLiteral("status"));
        QTextStream out(stdout);
        if (reply.isEmpty()) {
            out << "{\"schemaVersion\":1,\"running\":false}\n";
            return 1;
        }
        out << QString::fromUtf8(reply) << '\n';
        return 0;
    }

    QString line = QStringLiteral("spotlight ") + request.command;
    if (!request.argument.isEmpty())
        line += QLatin1Char(' ') + request.argument;
    return ShellIpcClient::send(QString::fromLatin1(kShellIpcName), line) ? 0 : 1;
}
