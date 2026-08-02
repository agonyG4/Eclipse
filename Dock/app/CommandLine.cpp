#include "app/CommandLine.hpp"

CommandLineRequest CommandLine::parse(const QStringList &args)
{
    CommandLineRequest request;
    for (const QString &arg : args) {
        if (arg == QStringLiteral("--daemon")) {
            request.daemonMode = true;
            request.mode = CommandLineRequest::Mode::Daemon;
        } else if (arg == QStringLiteral("--status")) {
            request.mode = CommandLineRequest::Mode::Status;
        } else if (arg == QStringLiteral("--reload")) {
            request.mode = CommandLineRequest::Mode::Reload;
        } else if (arg == QStringLiteral("--show")) {
            request.mode = CommandLineRequest::Mode::Show;
        } else if (arg == QStringLiteral("--hide")) {
            request.mode = CommandLineRequest::Mode::Hide;
        } else if (arg == QStringLiteral("--quit")) {
            request.mode = CommandLineRequest::Mode::Quit;
        }
    }
    return request;
}
