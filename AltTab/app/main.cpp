#include "app/CommandLine.hpp"
#include "platform/ipc/ShellIpcClient.hpp"

#include <QCoreApplication>
#include <QTextStream>

namespace {
constexpr QLatin1StringView kShellIpcName("astrea-shell-v1");
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const CommandLineRequest request = CommandLine::parse(app.arguments().mid(1));
    if (request.daemonMode) {
        QTextStream(stderr) << "astrea-alt-tab is a compatibility IPC client; use astrea-shell\n";
        return 2;
    }

    QString line;
    switch (request.mode) {
    case CommandLineRequest::Mode::Status: {
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
    case CommandLineRequest::Mode::Next: line = QStringLiteral("alttab next"); break;
    case CommandLineRequest::Mode::Previous: line = QStringLiteral("alttab previous"); break;
    case CommandLineRequest::Mode::Commit: line = QStringLiteral("alttab commit"); break;
    case CommandLineRequest::Mode::Cancel: line = QStringLiteral("alttab cancel"); break;
    case CommandLineRequest::Mode::Show: line = QStringLiteral("alttab show"); break;
    case CommandLineRequest::Mode::Hide: line = QStringLiteral("alttab hide"); break;
    case CommandLineRequest::Mode::ReloadWindows: line = QStringLiteral("alttab reload"); break;
    case CommandLineRequest::Mode::Daemon:
    case CommandLineRequest::Mode::Unknown:
        return 2;
    }
    return ShellIpcClient::send(QString::fromLatin1(kShellIpcName), line) ? 0 : 1;
}
