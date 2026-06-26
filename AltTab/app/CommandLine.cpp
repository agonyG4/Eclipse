#include "app/CommandLine.hpp"

CommandLineRequest CommandLine::parse(const QStringList &args)
{
    CommandLineRequest request;
    request.daemonMode = true;
    request.mode = CommandLineRequest::Mode::Daemon;

    for (int i = 0; i < args.size(); ++i) {
        const QString &arg = args.at(i);
        if (arg == QStringLiteral("--daemon")) {
            request.daemonMode = true;
            request.mode = CommandLineRequest::Mode::Daemon;
        } else if (arg == QStringLiteral("--next")) {
            request.mode = CommandLineRequest::Mode::Next;
            request.daemonMode = false;
        } else if (arg == QStringLiteral("--previous")) {
            request.mode = CommandLineRequest::Mode::Previous;
            request.daemonMode = false;
        } else if (arg == QStringLiteral("--commit")) {
            request.mode = CommandLineRequest::Mode::Commit;
            request.daemonMode = false;
        } else if (arg == QStringLiteral("--cancel")) {
            request.mode = CommandLineRequest::Mode::Cancel;
            request.daemonMode = false;
        } else if (arg == QStringLiteral("--show")) {
            request.mode = CommandLineRequest::Mode::Show;
            request.daemonMode = false;
        } else if (arg == QStringLiteral("--hide")) {
            request.mode = CommandLineRequest::Mode::Hide;
            request.daemonMode = false;
        } else if (arg == QStringLiteral("--reload-windows")) {
            request.mode = CommandLineRequest::Mode::ReloadWindows;
            request.daemonMode = false;
        } else if (arg == QStringLiteral("--status")) {
            request.mode = CommandLineRequest::Mode::Status;
            request.daemonMode = false;
        } else if (arg == QStringLiteral("--backend")) {
            if (i + 1 < args.size())
                request.backend = args.at(++i);
        } else if (arg.startsWith(QStringLiteral("--backend="))) {
            request.backend = arg.mid(10);
        }
    }

    return request;
}
