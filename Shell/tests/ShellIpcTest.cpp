#include <QCoreApplication>
#include <QLocalServer>
#include <QSignalSpy>
#include <QTest>

#include "platform/ipc/ShellIpcServer.hpp"

class ShellIpcTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesBoundedFeatureCommands();
    void roundTripsOneEndpoint();
};

void ShellIpcTest::parsesBoundedFeatureCommands()
{
    const auto command = ShellIpcServer::parseCommand(QStringLiteral("spotlight query app launcher"));
    QVERIFY(command.valid);
    QCOMPARE(command.feature, QStringLiteral("spotlight"));
    QCOMPARE(command.action, QStringLiteral("query"));
    QCOMPARE(command.argument, QStringLiteral("app launcher"));

    const auto status = ShellIpcServer::parseCommand(QStringLiteral("status"));
    QVERIFY(status.valid);
    QCOMPARE(status.feature, QStringLiteral("shell"));
    QCOMPARE(status.action, QStringLiteral("status"));

    QVERIFY(!ShellIpcServer::parseCommand(QStringLiteral("spotlight")).valid);
    QVERIFY(!ShellIpcServer::parseCommand(QStringLiteral("x y ") + QString(4096, QLatin1Char('a'))).valid);
}

void ShellIpcTest::roundTripsOneEndpoint()
{
    const QString name = QStringLiteral("astrea-shell-test-ipc");
    QLocalServer::removeServer(name);
    ShellIpcServer server;
    QSignalSpy received(&server, &ShellIpcServer::commandReceived);
    server.setReplyCallback([](const ShellIpcServer::Command &command) {
        return command.action == QStringLiteral("status")
            ? QStringLiteral("running=true;apps=3") : QString();
    });
    QVERIFY(server.listen(name));

    ShellIpcServer::Command query{true, QStringLiteral("spotlight"), QStringLiteral("query"),
                                  QStringLiteral("fire fox")};
    QVERIFY(ShellIpcServer::sendCommand(name, query));
    QTRY_COMPARE(received.count(), 1);
    QCOMPARE(received.at(0).at(0).value<ShellIpcServer::Command>().argument,
             QStringLiteral("fire fox"));

    const ShellIpcServer::Command status{true, QStringLiteral("shell"), QStringLiteral("status"), {}};
    QCOMPARE(QString::fromUtf8(ShellIpcServer::requestReply(name, status)).trimmed(),
             QStringLiteral("running=true;apps=3"));
    server.stopListening();
    QLocalServer::removeServer(name);
}

QTEST_GUILESS_MAIN(ShellIpcTest)
#include "ShellIpcTest.moc"
