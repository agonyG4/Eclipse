#include "app/CommandLine.hpp"

CommandLineRequest CommandLine::parse(const QStringList &args)
{
    CommandLineRequest request;
    request.mode = CommandLineRequest::Mode::SendCommand;
    request.command = QStringLiteral("show");

    for (int i = 0; i < args.size(); ++i) {
        const QString &arg = args.at(i);
        if (arg == QStringLiteral("--daemon")) {
            request.daemonMode = true;
            request.mode = CommandLineRequest::Mode::Daemon;
            request.command = QStringLiteral("hide");
        } else if (arg == QStringLiteral("--show")) {
            request.command = QStringLiteral("show");
        } else if (arg == QStringLiteral("--hide")) {
            request.command = QStringLiteral("hide");
        } else if (arg == QStringLiteral("--toggle")) {
            request.command = QStringLiteral("toggle");
        } else if (arg == QStringLiteral("--activate")) {
            request.command = QStringLiteral("activate");
        } else if (arg == QStringLiteral("--reload-index")) {
            request.command = QStringLiteral("reload-index");
        } else if (arg == QStringLiteral("--status")) {
            request.mode = CommandLineRequest::Mode::Status;
            request.command = QStringLiteral("status");
        } else if (arg == QStringLiteral("--icon-theme")) {
            request.mode = CommandLineRequest::Mode::IconThemeDiagnostics;
        } else if (arg == QStringLiteral("--resolve-icon") && i + 1 < args.size()) {
            request.mode = CommandLineRequest::Mode::ResolveIcon;
            request.argument = args.at(++i);
        } else if (arg.startsWith(QStringLiteral("--resolve-icon="))) {
            request.mode = CommandLineRequest::Mode::ResolveIcon;
            request.argument = arg.mid(15);
        } else if (arg == QStringLiteral("--query") && i + 1 < args.size()) {
            request.command = QStringLiteral("query");
            request.argument = args.at(++i);
        } else if (arg.startsWith(QStringLiteral("--query="))) {
            request.command = QStringLiteral("query");
            request.argument = arg.mid(8);
        }
    }

    return request;
}
