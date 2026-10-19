#include "platform/DownloadServer.hpp"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <cstdio>

using namespace Astrea::Downloads;

namespace {
void print(const QJsonObject &object)
{
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stdout);
    std::fflush(stdout);
}

int error(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("astrea-downloads");
    app.setApplicationVersion("1");
    QCommandLineParser parser;
    parser.setApplicationDescription("Local download progress API (JSON Lines, no browser capture)");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption socketOption("socket", "Absolute local socket path", "path");
    parser.addOption(socketOption);
    parser.addPositionalArgument("command", "serve | list | watch | update JSON | remove SOURCE ID");
    parser.addPositionalArgument("arguments", "Command arguments", "[arguments...]");
    parser.process(app);
    const auto args = parser.positionalArguments();
    if (args.isEmpty())
        parser.showHelp(1);
    const auto command = args.first();
    QJsonObject request{{"version", 1}, {"op", command}};
    if (command == "update" && args.size() == 2) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(args.at(1).toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
            return error("update requires a JSON download object");
        request["download"] = document.object();
    } else if (command == "remove" && args.size() == 3) {
        request["source"] = args.at(1);
        request["id"] = args.at(2);
    } else if (!((command == "serve" || command == "list" || command == "watch")
                 && args.size() == 1)) {
        return error("Usage: astrea-downloads [--socket PATH] serve|list|watch|update JSON|remove SOURCE ID");
    }

    QString path = parser.value(socketOption);
    if (!parser.isSet(socketOption)) {
        const auto runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if (runtime.isEmpty())
            return error("No user runtime directory; supply --socket PATH");
        path = QDir(runtime).filePath("astrea-downloads.sock");
    }
    if (!QFileInfo(path).isAbsolute() || path.contains(QChar::Null))
        return error("--socket must be an absolute path");
    if (command == "serve") {
        DownloadServer server;
        QString message;
        if (!server.listen(path, &message))
            return error(message);
        print({{"version", 1}, {"ok", true}, {"event", "ready"}, {"socket", path}});
        return app.exec();
    }

    QLocalSocket socket;
    socket.connectToServer(path);
    if (!socket.waitForConnected(2000))
        return error("Cannot connect to download service: " + socket.errorString());
    const auto frame = QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
    if (frame.size() - 1 > DownloadServer::MaxFrameBytes)
        return error("Request exceeds the 64 KiB frame limit");
    socket.write(frame);
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &app, [&app] {
        error("Timed out waiting for the download service");
        app.exit(1);
    });
    timeout.start(3000);
    QByteArray buffer;
    bool receivedReply = false;
    QObject::connect(&socket, &QLocalSocket::readyRead, &app, [&] {
        buffer += socket.readAll();
        while (true) {
            const auto newline = buffer.indexOf('\n');
            if (newline < 0)
                break;
            const auto document = QJsonDocument::fromJson(buffer.left(newline));
            buffer.remove(0, newline + 1);
            if (!document.isObject()) {
                error("Invalid reply from download service");
                app.exit(1);
                return;
            }
            receivedReply = true;
            timeout.stop();
            const auto response = document.object();
            print(response);
            if (!response.value("ok").toBool() || command != "watch") {
                app.exit(response.value("ok").toBool() ? 0 : 1);
                return;
            }
        }
        // A full list can exceed the per-request frame limit.
        if (buffer.size() > 32 * 1024 * 1024) {
            error("Reply exceeds the client size limit");
            app.exit(1);
        }
    });
    QObject::connect(&socket, &QLocalSocket::disconnected, &app, [&] {
        error(receivedReply ? "Download service disconnected" : "Service disconnected before replying");
        app.exit(1);
    });
    return app.exec();
}
