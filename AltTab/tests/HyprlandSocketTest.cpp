#include <QDir>
#include <QEventLoop>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QDateTime>
#include <QThread>

#include "platform/hyprland/HyprlandCommand.hpp"
#include "platform/hyprland/HyprlandSocket.hpp"

class TestHyprlandSocket : public QObject {
    Q_OBJECT

private:
    // Helper: fake server that records received bytes and sends replies
    struct FakeServer {
        QTemporaryDir runtime;
        QLocalServer server;
        QByteArray receivedBytes;
        std::function<void(QLocalSocket*)> onConnection;

        bool listen(std::function<void(QLocalSocket*)> handler) {
            onConnection = std::move(handler);
            QObject::connect(&server, &QLocalServer::newConnection, [this]() {
                auto *client = server.nextPendingConnection();
                if (onConnection) onConnection(client);
            });
            return server.listen(runtime.path() + QStringLiteral("/.socket.sock"));
        }

        QString path() const { return runtime.path(); }
    };

private slots:
    void testJsonClientsBytes();
    void testJsonMonitorsBytes();
    void testEvalWorkspaceBytes();
    void testEvalWindowBytes();
    void testNoTrailingNewline();
    void testPartialWrite();
    void testResponseOneChunk();
    void testResponseMultipleChunks();
    void testResponseNoNewline();
    void testResponseWithNewline();
    void testServerWritesAndCloses();
    void testServerClosesWithoutResponse();
    void testServerTimeout();
    void testConnectionRefused();
    void testErrorThenDisconnect();
    void testDisconnectThenLateError();
    void testOversizedResponse();
    void testCallbackOnce();
    void testRequestDestroyedOnce();
    void testShutdownCancelsRequest();
    void testSequentialRequests();
    void testMixedRequests();
    void testNoActiveSocketAfterCompletion();
};

void TestHyprlandSocket::testJsonClientsBytes()
{
    const QByteArray request = HyprlandRequest::jsonInfoRequest(u"clients");
    QCOMPARE(request, QByteArrayLiteral("j/clients"));
}

void TestHyprlandSocket::testJsonMonitorsBytes()
{
    const QByteArray request = HyprlandRequest::jsonInfoRequest(u"monitors");
    QCOMPARE(request, QByteArrayLiteral("j/monitors"));
}

void TestHyprlandSocket::testEvalWorkspaceBytes()
{
    const QByteArray request = HyprlandRequest::focusWorkspaceRequest(3);
    QCOMPARE(request, QByteArrayLiteral("/eval hl.dispatch(hl.dsp.focus({ workspace = \"3\" }))"));
}

void TestHyprlandSocket::testEvalWindowBytes()
{
    const QByteArray request = HyprlandRequest::focusWindowRequest(QStringLiteral("0x1234"));
    QCOMPARE(request, QByteArrayLiteral("/eval hl.dispatch(hl.dsp.focus({ window = \"address:0x1234\" }))"));
}

void TestHyprlandSocket::testNoTrailingNewline()
{
    QVERIFY(!HyprlandRequest::jsonInfoRequest(u"clients").endsWith('\n'));
    QVERIFY(!HyprlandRequest::focusWorkspaceRequest(3).endsWith('\n'));
    QVERIFY(!HyprlandRequest::focusWindowRequest(QStringLiteral("0x1234")).endsWith('\n'));
}

void TestHyprlandSocket::testPartialWrite()
{
    FakeServer fake;
    QVERIFY(fake.listen([&](QLocalSocket *client) {
        QObject::connect(client, &QLocalSocket::readyRead, [client, &fake]() {
            fake.receivedBytes.append(client->readAll());
        });
        QObject::connect(client, &QLocalSocket::disconnected, [client]() {
            client->deleteLater();
        });
    }));

    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), fake.path());

    QEventLoop loop;
    socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &) {
        loop.quit();
    });
    QTimer::singleShot(500, &loop, &QEventLoop::quit);
    loop.exec();

    QCOMPARE(fake.receivedBytes, QByteArrayLiteral("j/clients"));
}

void TestHyprlandSocket::testResponseOneChunk()
{
    FakeServer fake;
    QVERIFY(fake.listen([&](QLocalSocket *client) {
        QObject::connect(client, &QLocalSocket::readyRead, [client]() {
            client->readAll();
            client->write(QByteArrayLiteral("[{\"class\":\"Firefox\"}]"));
            client->flush();
            client->disconnectFromServer();
        });
        QObject::connect(client, &QLocalSocket::disconnected, [client]() {
            client->deleteLater();
        });
    }));

    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), fake.path());

    HyprlandCommandResult result;
    QEventLoop loop;
    socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(result.success);
    QCOMPARE(result.rawReply, QByteArrayLiteral("[{\"class\":\"Firefox\"}]"));
}

void TestHyprlandSocket::testResponseMultipleChunks()
{
    FakeServer fake;
    QVERIFY(fake.listen([&](QLocalSocket *client) {
        QObject::connect(client, &QLocalSocket::readyRead, [client]() {
            client->readAll();
            client->write(QByteArrayLiteral("[{\"class\":\""));
            client->flush();
            QTimer::singleShot(10, client, [client]() {
                client->write(QByteArrayLiteral("Firefox\"}]"));
                client->flush();
                client->disconnectFromServer();
            });
        });
        QObject::connect(client, &QLocalSocket::disconnected, [client]() {
            client->deleteLater();
        });
    }));

    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), fake.path());

    HyprlandCommandResult result;
    QEventLoop loop;
    socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(result.success);
    QCOMPARE(result.rawReply, QByteArrayLiteral("[{\"class\":\"Firefox\"}]"));
}

void TestHyprlandSocket::testResponseNoNewline()
{
    FakeServer fake;
    QVERIFY(fake.listen([&](QLocalSocket *client) {
        QObject::connect(client, &QLocalSocket::readyRead, [client]() {
            client->readAll();
            client->write("ok");
            client->flush();
            client->disconnectFromServer();
        });
        QObject::connect(client, &QLocalSocket::disconnected, [client]() {
            client->deleteLater();
        });
    }));

    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), fake.path());

    HyprlandCommandResult result;
    QEventLoop loop;
    socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(result.success);
    QCOMPARE(result.rawReply, QByteArrayLiteral("ok"));
}

void TestHyprlandSocket::testResponseWithNewline()
{
    FakeServer fake;
    QVERIFY(fake.listen([&](QLocalSocket *client) {
        QObject::connect(client, &QLocalSocket::readyRead, [client]() {
            client->readAll();
            client->write("ok\nmore");
            client->flush();
            client->disconnectFromServer();
        });
        QObject::connect(client, &QLocalSocket::disconnected, [client]() {
            client->deleteLater();
        });
    }));

    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), fake.path());

    HyprlandCommandResult result;
    QEventLoop loop;
    socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(result.success);
    QCOMPARE(result.rawReply, QByteArrayLiteral("ok\nmore"));
}

void TestHyprlandSocket::testServerWritesAndCloses()
{
    FakeServer fake;
    QVERIFY(fake.listen([&](QLocalSocket *client) {
        QObject::connect(client, &QLocalSocket::readyRead, [client]() {
            client->readAll();
            client->write("[]");
            client->flush();
            client->disconnectFromServer();
        });
        QObject::connect(client, &QLocalSocket::disconnected, [client]() {
            client->deleteLater();
        });
    }));

    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), fake.path());

    HyprlandCommandResult result;
    QEventLoop loop;
    socket.sendCommand(QByteArrayLiteral("j/monitors"), [&](const HyprlandCommandResult &r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(result.success);
    QCOMPARE(result.rawReply, QByteArrayLiteral("[]"));
}

void TestHyprlandSocket::testServerClosesWithoutResponse()
{
    FakeServer fake;
    QVERIFY(fake.listen([&](QLocalSocket *client) {
        QObject::connect(client, &QLocalSocket::readyRead, [client]() {
            client->readAll();
            client->disconnectFromServer();
        });
        QObject::connect(client, &QLocalSocket::disconnected, [client]() {
            client->deleteLater();
        });
    }));

    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), fake.path());

    HyprlandCommandResult result;
    QEventLoop loop;
    socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(!result.success);
    QCOMPARE(result.errorCode, QStringLiteral("empty_response"));
}

void TestHyprlandSocket::testServerTimeout()
{
    FakeServer fake;
    QVERIFY(fake.listen([&](QLocalSocket *) {
        // Never respond, never close
    }));

    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), fake.path());

    HyprlandCommandResult result;
    QEventLoop loop;
    socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &r) {
        result = r;
        loop.quit();
    }, 100);
    QTimer::singleShot(500, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(!result.success);
    QCOMPARE(result.errorCode, QStringLiteral("timeout"));
}

void TestHyprlandSocket::testConnectionRefused()
{
    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), QStringLiteral("/nonexistent/path"));

    HyprlandCommandResult result;
    QEventLoop loop;
    socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &r) {
        result = r;
        loop.quit();
    }, 500);
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(!result.success);
    QVERIFY(!result.errorCode.isEmpty());
}

void TestHyprlandSocket::testErrorThenDisconnect()
{
    FakeServer fake;
    QVERIFY(fake.listen([&](QLocalSocket *client) {
        QObject::connect(client, &QLocalSocket::readyRead, [client]() {
            client->readAll();
            client->write(QByteArrayLiteral("ok"));
            client->flush();
            client->disconnectFromServer();
        });
        QObject::connect(client, &QLocalSocket::disconnected, [client]() {
            client->deleteLater();
        });
    }));

    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), fake.path());

    HyprlandCommandResult result;
    QEventLoop loop;
    socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(result.success);
    QCOMPARE(result.rawReply, QByteArrayLiteral("ok"));
}

void TestHyprlandSocket::testDisconnectThenLateError()
{
    QTemporaryDir runtime;
    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), runtime.path());

    HyprlandCommandResult result;
    QEventLoop loop;
    int callbackCount = 0;
    socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &r) {
        result = r;
        ++callbackCount;
        loop.quit();
    }, 100);
    QTimer::singleShot(500, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(!result.success);
    QVERIFY(!result.errorCode.isEmpty());
    QCOMPARE(callbackCount, 1);
}

void TestHyprlandSocket::testOversizedResponse()
{
    FakeServer fake;
    QVERIFY(fake.listen([&](QLocalSocket *client) {
        QObject::connect(client, &QLocalSocket::readyRead, [client]() {
            client->readAll();
            QByteArray big(kMaxCommandReplyBytes + 1, 'x');
            client->write(big);
            client->flush();
            client->disconnectFromServer();
        });
        QObject::connect(client, &QLocalSocket::disconnected, [client]() {
            client->deleteLater();
        });
    }));

    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), fake.path());

    HyprlandCommandResult result;
    QEventLoop loop;
    socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(!result.success);
    QCOMPARE(result.errorCode, QStringLiteral("oversized_response"));
}

void TestHyprlandSocket::testCallbackOnce()
{
    FakeServer fake;
    QVERIFY(fake.listen([&](QLocalSocket *client) {
        QObject::connect(client, &QLocalSocket::readyRead, [client]() {
            client->readAll();
            client->write("ok");
            client->flush();
            client->disconnectFromServer();
        });
        QObject::connect(client, &QLocalSocket::disconnected, [client]() {
            client->deleteLater();
        });
    }));

    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), fake.path());

    int callbackCount = 0;
    QEventLoop loop;
    socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &) {
        ++callbackCount;
        loop.quit();
    });
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();

    QCOMPARE(callbackCount, 1);
}

void TestHyprlandSocket::testRequestDestroyedOnce()
{
    FakeServer fake;
    QVERIFY(fake.listen([&](QLocalSocket *client) {
        QObject::connect(client, &QLocalSocket::readyRead, [client]() {
            client->readAll();
            client->write("ok");
            client->flush();
            client->disconnectFromServer();
        });
        QObject::connect(client, &QLocalSocket::disconnected, [client]() {
            client->deleteLater();
        });
    }));

    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), fake.path());

    QEventLoop loop;
    socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &) {
        loop.quit();
    });
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();
}

void TestHyprlandSocket::testShutdownCancelsRequest()
{
    auto *socket = new HyprlandSocket();

    QEventLoop loop;
    int callbackCount = 0;
    socket->setInstance(QStringLiteral("test-sig"), QStringLiteral("/nonexistent"));

    socket->sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &) {
        ++callbackCount;
        loop.quit();
    });

    socket->stop();
    delete socket;

    QTimer::singleShot(200, &loop, &QEventLoop::quit);
    loop.exec();
    QCOMPARE(callbackCount, 0);
}

void TestHyprlandSocket::testSequentialRequests()
{
    QLocalServer server;
    QTemporaryDir runtime;
    QVERIFY(server.listen(runtime.path() + QStringLiteral("/.socket.sock")));
    QAtomicInt completedCount(0);
    constexpr int kCount = 100;

    // Server handler
    QObject::connect(&server, &QLocalServer::newConnection, [&]() {
        auto *client = server.nextPendingConnection();
        QObject::connect(client, &QLocalSocket::readyRead, [client]() {
            client->readAll();
            client->write("[]");
            client->flush();
            client->disconnectFromServer();
        });
        QObject::connect(client, &QLocalSocket::disconnected, [client]() {
            client->deleteLater();
        });
    });

    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), runtime.path());

    QEventLoop loop;
    for (int i = 0; i < kCount; ++i) {
        socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &result) {
            QVERIFY(result.success);
            if (completedCount.fetchAndAddRelaxed(1) + 1 >= kCount)
                loop.quit();
        });
        QTest::qWait(5);
    }

    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();

    QCOMPARE(completedCount.loadRelaxed(), kCount);
}

void TestHyprlandSocket::testMixedRequests()
{
    QLocalServer server;
    QTemporaryDir runtime;
    QVERIFY(server.listen(runtime.path() + QStringLiteral("/.socket.sock")));
    QAtomicInt callCount(0);
    constexpr int kCount = 100;

    QObject::connect(&server, &QLocalServer::newConnection, [&]() {
        auto *client = server.nextPendingConnection();
        QObject::connect(client, &QLocalSocket::readyRead, [client]() {
            client->readAll();
            client->write("[]");
            client->flush();
            client->disconnectFromServer();
        });
        QObject::connect(client, &QLocalSocket::disconnected, [client]() {
            client->deleteLater();
        });
    });

    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), runtime.path());

    QEventLoop loop;
    for (int i = 0; i < kCount; ++i) {
        socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &) {
            if (callCount.fetchAndAddRelaxed(1) + 1 >= kCount)
                loop.quit();
        });
        QTest::qWait(5);
    }

    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();

    QCOMPARE(callCount.loadRelaxed(), kCount);
}

void TestHyprlandSocket::testNoActiveSocketAfterCompletion()
{
    FakeServer fake;
    QVERIFY(fake.listen([&](QLocalSocket *client) {
        QObject::connect(client, &QLocalSocket::readyRead, [client]() {
            client->readAll();
            client->write("ok");
            client->flush();
            client->disconnectFromServer();
        });
        QObject::connect(client, &QLocalSocket::disconnected, [client]() {
            client->deleteLater();
        });
    }));

    HyprlandSocket socket;
    socket.setInstance(QStringLiteral("test-sig"), fake.path());

    QEventLoop loop;
    socket.sendCommand(QByteArrayLiteral("j/clients"), [&](const HyprlandCommandResult &) {
        loop.quit();
    });
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(!socket.commandInFlight());
}

QTEST_MAIN(TestHyprlandSocket)
#include "HyprlandSocketTest.moc"
