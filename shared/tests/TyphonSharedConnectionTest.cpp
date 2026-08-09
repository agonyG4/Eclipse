#include <QSignalSpy>
#include <QTest>

#include "platform/typhon/TyphonSharedConnection.hpp"

#include <functional>

class TestTyphonSharedConnection final : public QObject {
    Q_OBJECT

private slots:
    void authenticatesOncePerGeneration();
    void repeatedStartAndDisconnectAreIdempotent();
};

void TestTyphonSharedConnection::authenticatesOncePerGeneration()
{
    int connectCount = 0;
    int authenticateCount = 0;
    std::function<void()> disconnectCallback;

    TyphonSharedConnection::Hooks hooks;
    hooks.connect = [&connectCount] {
        ++connectCount;
        return true;
    };
    hooks.authenticate = [&authenticateCount](QString *) {
        ++authenticateCount;
        return true;
    };
    hooks.disconnect = [] {};
    hooks.setDisconnectHandler = [&disconnectCallback](std::function<void()> callback) {
        disconnectCallback = std::move(callback);
    };

    TyphonSharedConnection connection(hooks);
    QSignalSpy readySpy(&connection, &TyphonSharedConnection::ready);

    connection.start();

    QCOMPARE(connectCount, 1);
    QCOMPARE(authenticateCount, 1);
    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(connection.connectionGeneration(), quint64(1));
    QCOMPARE(connection.authenticationGeneration(), quint64(1));

    QVERIFY(disconnectCallback);
    disconnectCallback();
    connection.reconnectNowForTest();

    QCOMPARE(connectCount, 2);
    QCOMPARE(authenticateCount, 2);
    QCOMPARE(readySpy.count(), 2);
    QCOMPARE(connection.connectionGeneration(), quint64(2));
    QCOMPARE(connection.authenticationGeneration(), quint64(2));
}

void TestTyphonSharedConnection::repeatedStartAndDisconnectAreIdempotent()
{
    int connectCount = 0;
    int authenticateCount = 0;
    int disconnectCount = 0;

    TyphonSharedConnection::Hooks hooks;
    hooks.connect = [&connectCount] {
        ++connectCount;
        return true;
    };
    hooks.authenticate = [&authenticateCount](QString *) {
        ++authenticateCount;
        return true;
    };
    hooks.disconnect = [&disconnectCount] { ++disconnectCount; };
    hooks.setDisconnectHandler = [](std::function<void()>) {};

    TyphonSharedConnection connection(hooks);
    connection.start();
    connection.start();
    connection.stop();
    connection.stop();

    QCOMPARE(connectCount, 1);
    QCOMPARE(authenticateCount, 1);
    QCOMPARE(disconnectCount, 1);
}

QTEST_MAIN(TestTyphonSharedConnection)
#include "TyphonSharedConnectionTest.moc"
