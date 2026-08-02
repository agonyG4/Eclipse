#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QSignalSpy>
#include <QTest>

#include "platform/ipc/DockIpcServer.hpp"

class DockIpcTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesEveryCommand();
    void rejectsMalformedMessages();
    void serverDispatchesCommandsAndStatus();
    void staleSocketIsRecovered();
    void alreadyRunningServerIsNotReplaced();
    void clientTimeoutIsBounded();
};

void DockIpcTest::parsesEveryCommand()
{
    QCOMPARE(DockIpcServer::parseCommand(QStringLiteral("status")).type,
             DockIpcServer::Command::Status);
    QCOMPARE(DockIpcServer::parseCommand(QStringLiteral("reload")).type,
             DockIpcServer::Command::Reload);
    QCOMPARE(DockIpcServer::parseCommand(QStringLiteral("show")).type,
             DockIpcServer::Command::Show);
    QCOMPARE(DockIpcServer::parseCommand(QStringLiteral("hide")).type,
             DockIpcServer::Command::Hide);
    QCOMPARE(DockIpcServer::parseCommand(QStringLiteral("quit")).type,
             DockIpcServer::Command::Quit);
    QCOMPARE(DockIpcServer::parseCommand(QStringLiteral("--show")).type,
             DockIpcServer::Command::Show);
}

void DockIpcTest::rejectsMalformedMessages()
{
    QCOMPARE(DockIpcServer::parseCommand(QStringLiteral("status extra")).type,
             DockIpcServer::Command::Unknown);
    QCOMPARE(DockIpcServer::parseCommand(QStringLiteral("reload-index")).type,
             DockIpcServer::Command::Unknown);
    QCOMPARE(DockIpcServer::parseCommand(QStringLiteral("{\"command\":\"show\"}")).type,
             DockIpcServer::Command::Unknown);
    QCOMPARE(DockIpcServer::parseCommand(QString()).type,
             DockIpcServer::Command::Unknown);
}

void DockIpcTest::serverDispatchesCommandsAndStatus()
{
    const QString name = QStringLiteral("astrea-dock-test-%1").arg(QCoreApplication::applicationPid());
    DockIpcServer server;
    QVERIFY(server.listen(name));
    server.setReplyCallback([] { return QStringLiteral("{\"schemaVersion\":1}"); });
    QSignalSpy commandSpy(&server, &DockIpcServer::commandReceived);

    QVERIFY(DockIpcServer::sendCommand(name, {DockIpcServer::Command::Show}));
    QTRY_COMPARE(commandSpy.count(), 1);
    QCOMPARE(qvariant_cast<DockIpcServer::IpcCommand>(commandSpy.at(0).at(0)).type,
             DockIpcServer::Command::Show);

    const QByteArray reply = DockIpcServer::requestReply(
        name, {DockIpcServer::Command::Status}, 300);
    const QJsonDocument document = QJsonDocument::fromJson(reply);
    QVERIFY(document.isObject());
    QCOMPARE(document.object().value(QStringLiteral("schemaVersion")).toInt(), 1);
    server.stopListening();
}

void DockIpcTest::staleSocketIsRecovered()
{
    const QString name = QStringLiteral("astrea-dock-stale-%1").arg(QCoreApplication::applicationPid());
    QLocalServer stale;
    QVERIFY(stale.listen(name));
    stale.close();

    DockIpcServer server;
    QVERIFY(server.listen(name));
    server.stopListening();
}

void DockIpcTest::alreadyRunningServerIsNotReplaced()
{
    const QString name = QStringLiteral("astrea-dock-running-%1").arg(QCoreApplication::applicationPid());
    DockIpcServer first;
    DockIpcServer second;
    QVERIFY(first.listen(name));
    QVERIFY(!second.listen(name));
    first.stopListening();
}

void DockIpcTest::clientTimeoutIsBounded()
{
    QElapsedTimer timer;
    timer.start();
    const QByteArray reply = DockIpcServer::requestReply(
        QStringLiteral("astrea-dock-no-server"), {DockIpcServer::Command::Status}, 80);
    QVERIFY(reply.isEmpty());
    QVERIFY(timer.elapsed() < 500);
}

QTEST_MAIN(DockIpcTest)
#include "DockIpcTest.moc"
