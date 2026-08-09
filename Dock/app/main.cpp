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
        QTextStream(stderr) << "astrea-dock is a compatibility IPC client; use astrea-shell\n";
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
    case CommandLineRequest::Mode::Reload: line = QStringLiteral("dock reload"); break;
    case CommandLineRequest::Mode::Show: line = QStringLiteral("dock show"); break;
    case CommandLineRequest::Mode::Hide: line = QStringLiteral("dock hide"); break;
    case CommandLineRequest::Mode::Quit: line = QStringLiteral("shell quit"); break;
    case CommandLineRequest::Mode::Daemon: return 2;
    }
    return ShellIpcClient::send(QString::fromLatin1(kShellIpcName), line) ? 0 : 1;
}
