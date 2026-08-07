#include <QTest>
#include <QSignalSpy>
#include <QLocalSocket>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

#include "platform/ipc/AltTabIpcServer.hpp"

class TestAltTabIpc : public QObject {
    Q_OBJECT

private slots:
    void testParseCommands();
    void testLargeCommandRejected();
    void testStatusResponse();
    void testMalformedCommand();
    void testDuplicateDaemon();
    void testStaleSocketIsRecovered();
    void testIdempotentCommit();
};

void TestAltTabIpc::testParseCommands() {
    auto test = [](const QString &input, AltTabIpcServer::Command expected, const QString &expectedText = {}) {
        auto cmd = AltTabIpcServer::parseCommand(input);
        QCOMPARE(static_cast<int>(cmd.type), static_cast<int>(expected));
        if (!expectedText.isEmpty())
            QCOMPARE(cmd.text, expectedText);
    };

    test(QStringLiteral("next"), AltTabIpcServer::Command::Next);
    test(QStringLiteral("previous"), AltTabIpcServer::Command::Previous);
    test(QStringLiteral("commit"), AltTabIpcServer::Command::Commit);
    test(QStringLiteral("cancel"), AltTabIpcServer::Command::Cancel);
    test(QStringLiteral("show"), AltTabIpcServer::Command::Show);
    test(QStringLiteral("hide"), AltTabIpcServer::Command::Hide);
    test(QStringLiteral("reload-windows"), AltTabIpcServer::Command::ReloadWindows);
    test(QStringLiteral("status"), AltTabIpcServer::Command::Status);
    test(QStringLiteral("--next"), AltTabIpcServer::Command::Next);
    test(QStringLiteral("--previous"), AltTabIpcServer::Command::Previous);
    test(QStringLiteral("--commit"), AltTabIpcServer::Command::Commit);
    test(QStringLiteral("--cancel"), AltTabIpcServer::Command::Cancel);
    test(QStringLiteral("--show"), AltTabIpcServer::Command::Show);
    test(QStringLiteral("--hide"), AltTabIpcServer::Command::Hide);
    test(QStringLiteral("--status"), AltTabIpcServer::Command::Status);
    test(QStringLiteral("--daemon"), AltTabIpcServer::Command::Hide);
    test(QStringLiteral("resolve-window-icon 0x1234"), AltTabIpcServer::Command::Unknown);
    test(QStringLiteral("--resolve-window-icon 0x5678"), AltTabIpcServer::Command::Unknown);
    test(QStringLiteral("unknown"), AltTabIpcServer::Command::Unknown);
    test(QString(), AltTabIpcServer::Command::Unknown);
}

void TestAltTabIpc::testLargeCommandRejected() {
    AltTabIpcServer server;
    QVERIFY(server.listen(QStringLiteral("test-large-rejected")));

    QLocalSocket socket;
    socket.connectToServer(QStringLiteral("test-large-rejected"));
    QVERIFY(socket.waitForConnected(500));

    QByteArray large(5000, 'A');
    large.append('\n');
    socket.write(large);
    QVERIFY(socket.waitForBytesWritten(200));

    QTest::qWait(100);

    socket.disconnectFromServer();
    QLocalServer::removeServer(QStringLiteral("test-large-rejected"));
}

void TestAltTabIpc::testStatusResponse() {
    AltTabIpcServer server;
    server.setReplyCallback([](const AltTabIpcServer::IpcCommand &) -> QString {
        return QStringLiteral("{\"running\":true,\"state\":\"hidden\"}");
    });
    QVERIFY(server.listen(QStringLiteral("test-status-response")));

    AltTabIpcServer::IpcCommand cmd;
    cmd.type = AltTabIpcServer::Command::Status;
    const QByteArray reply = AltTabIpcServer::requestReply(QStringLiteral("test-status-response"), cmd, 2000);
    QVERIFY(!reply.isEmpty());
    QJsonDocument doc = QJsonDocument::fromJson(reply);
    QVERIFY(doc.isObject());
    QCOMPARE(doc.object().value(QStringLiteral("running")).toBool(), true);
    QCOMPARE(doc.object().value(QStringLiteral("state")).toString(), QStringLiteral("hidden"));

    QLocalServer::removeServer(QStringLiteral("test-status-response"));
}

void TestAltTabIpc::testMalformedCommand() {
    AltTabIpcServer server;
    QSignalSpy cmdSpy(&server, &AltTabIpcServer::commandReceived);
    QVERIFY(server.listen(QStringLiteral("test-malformed")));

    QLocalSocket socket;
    socket.connectToServer(QStringLiteral("test-malformed"));
    QVERIFY(socket.waitForConnected(500));

    socket.write("invalid command here\n");
    QVERIFY(socket.waitForBytesWritten(200));
    QTest::qWait(100);

    QCOMPARE(cmdSpy.count(), 0);

    socket.disconnectFromServer();
    QLocalServer::removeServer(QStringLiteral("test-malformed"));
}

void TestAltTabIpc::testDuplicateDaemon() {
    AltTabIpcServer server1;
    QVERIFY(server1.listen(QStringLiteral("test-duplicate")));

    AltTabIpcServer server2;
    bool result = server2.listen(QStringLiteral("test-duplicate"));
    QVERIFY(!result);

    server1.stopListening();
}

void TestAltTabIpc::testStaleSocketIsRecovered() {
    QLocalServer staleServer;
    QVERIFY(staleServer.listen(QStringLiteral("test-stale-socket")));
    staleServer.close();

    AltTabIpcServer server;
    QVERIFY(server.listen(QStringLiteral("test-stale-socket")));
    server.stopListening();
}

void TestAltTabIpc::testIdempotentCommit() {
    AltTabIpcServer server;
    QSignalSpy cmdSpy(&server, &AltTabIpcServer::commandReceived);
    QVERIFY(server.listen(QStringLiteral("test-idempotent-commit")));

    QLocalSocket socket;
    socket.connectToServer(QStringLiteral("test-idempotent-commit"));
    QVERIFY(socket.waitForConnected(500));

    auto sendCmd = [&](const QString &text) {
        socket.write(text.toUtf8() + "\n");
        socket.waitForBytesWritten(100);
    };

    sendCmd(QStringLiteral("commit"));
    sendCmd(QStringLiteral("commit"));
    sendCmd(QStringLiteral("commit"));
    QTest::qWait(100);

    QCOMPARE(cmdSpy.count(), 3);

    for (int i = 0; i < cmdSpy.count(); ++i) {
        auto cmd = cmdSpy.at(i).at(0).value<AltTabIpcServer::IpcCommand>();
        QCOMPARE(static_cast<int>(cmd.type), static_cast<int>(AltTabIpcServer::Command::Commit));
    }

    socket.disconnectFromServer();
    QLocalServer::removeServer(QStringLiteral("test-idempotent-commit"));
}

QTEST_MAIN(TestAltTabIpc)
#include "AltTabIpcTest.moc"
