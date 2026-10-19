#include "core/DownloadStore.hpp"
#include "platform/DownloadServer.hpp"

#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QtTest>
#include <vector>

using namespace Astrea::Downloads;

namespace {
QJsonObject download(QString id = QStringLiteral("123"))
{
    return {{"source", "brave:default"}, {"id", id}, {"filename", "arquivo.zip"},
            {"state", "downloading"}, {"receivedBytes", 62}, {"totalBytes", 100}};
}

QJsonObject readReply(QLocalSocket &socket)
{
    QElapsedTimer timer;
    timer.start();
    while (!socket.canReadLine() && timer.elapsed() < 2000) {
        QCoreApplication::processEvents();
        QTest::qWait(1);
    }
    return QJsonDocument::fromJson(socket.readLine()).object();
}

void send(QLocalSocket &socket, QJsonObject request)
{
    socket.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
    socket.flush();
}
} // namespace

class DownloadProgressTest : public QObject {
    Q_OBJECT
private slots:
    void progressAndStates()
    {
        DownloadStore store;
        auto item = download();
        const auto reply = store.update(item);
        QVERIFY(reply.value("ok").toBool());
        QCOMPARE(reply.value("download").toObject().value("progress").toDouble(), 0.62);
        item["totalBytes"] = QJsonValue::Null;
        QVERIFY(store.update(item).value("download").toObject().value("progress").isNull());
        item["totalBytes"] = 0;
        QVERIFY(store.update(item).value("download").toObject().value("progress").isNull());
        item["state"] = "completed";
        QCOMPARE(store.update(item).value("download").toObject().value("progress").toDouble(), 1.0);
        for (const auto &state : {"downloading", "paused", "cancelled", "failed"}) {
            item["state"] = state;
            QVERIFY(store.update(item).value("ok").toBool());
            QCOMPARE(store.list().first().toObject().value("state").toString(), QString(state));
        }
        item["totalBytes"] = 1;
        QCOMPARE(store.update(item).value("download").toObject().value("progress").toDouble(), 1.0);
        QCOMPARE(store.list().first().toObject().value("state").toString(), QString("failed"));
    }

    void invalidUpdatesAreAtomic()
    {
        DownloadStore store;
        QVERIFY(store.update(download()).value("ok").toBool());
        const auto before = store.list();
        const QJsonObject invalid[] = {
            {{"receivedBytes", -1}}, {{"receivedBytes", 1.5}},
            {{"receivedBytes", "62"}}, {{"totalBytes", -1}},
            {{"totalBytes", 9007199254740992.0}}, {{"state", "bogus"}},
            {{"filename", ""}}, {{"source", ""}}, {{"id", ""}},
            {{"source", QString::fromLatin1("bad\0source", 10)}}
        };
        for (const auto &patch : invalid) {
            auto item = download();
            for (auto it = patch.begin(); it != patch.end(); ++it)
                item[it.key()] = it.value();
            QVERIFY(!store.update(item).value("ok").toBool());
            QCOMPARE(store.list(), before);
        }
        auto missing = download();
        missing.remove("totalBytes");
        QVERIFY(!store.update(missing).value("ok").toBool());
        QCOMPARE(store.list(), before);
    }

    void independentSourcesAndCapacity()
    {
        DownloadStore store;
        QVERIFY(store.update(download()).value("ok").toBool());
        auto zen = download();
        zen["source"] = "zen:default";
        QVERIFY(store.update(zen).value("ok").toBool());
        QCOMPARE(store.list().size(), 2);
        QVERIFY(store.remove("brave:default", "123").value("ok").toBool());
        QCOMPARE(store.list().first().toObject().value("source").toString(), QString("zen:default"));
        QVERIFY(!store.remove("brave:default", "123").value("ok").toBool());
        for (int i = 1; i < DownloadStore::MaxDownloads; ++i)
            QVERIFY(store.update(download(QString::number(i + 1000))).value("ok").toBool());
        QVERIFY(!store.update(download("extra")).value("ok").toBool());
        QVERIFY(store.update(zen).value("ok").toBool());
    }

    void metadataCapacityIsAtomicAndRecoverable()
    {
        DownloadStore store;
        auto item = download();
        item["filename"] = QString(4096, 'z');
        int inserted = 0;
        for (; inserted < DownloadStore::MaxDownloads; ++inserted) {
            item["id"] = QString::number(inserted);
            if (!store.update(item).value("ok").toBool())
                break;
        }
        QVERIFY(inserted > 0);
        QVERIFY(inserted < DownloadStore::MaxDownloads);
        QCOMPARE(store.list().size(), inserted);
        QVERIFY(store.remove("brave:default", "0").value("ok").toBool());
        QVERIFY(store.update(item).value("ok").toBool());
    }

    void socketProtocol()
    {
        QTemporaryDir directory(QCoreApplication::applicationDirPath() + "/downloads-test-XXXXXX");
        QVERIFY(directory.isValid());
        const auto path = directory.path() + "/api.sock";
        DownloadServer server;
        QString error;
        QVERIFY2(server.listen(path, &error), qPrintable(error));
        DownloadServer duplicate;
        QVERIFY(!duplicate.listen(path, &error));

        QLocalSocket watcher;
        watcher.connectToServer(path);
        QVERIFY(watcher.waitForConnected());
        send(watcher, {{"version", 1}, {"op", "watch"}});
        const auto snapshot = readReply(watcher);
        QCOMPARE(snapshot.value("event").toString(), QString("snapshot"));
        QVERIFY(snapshot.value("downloads").toArray().isEmpty());

        QLocalSocket producer;
        producer.connectToServer(path);
        QVERIFY(producer.waitForConnected());
        send(producer, {{"version", 1}, {"op", "update"}, {"download", download()}});
        QVERIFY(readReply(producer).value("ok").toBool());
        const auto event = readReply(watcher);
        QCOMPARE(event.value("event").toString(), QString("updated"));
        QCOMPARE(event.value("download").toObject().value("progress").toDouble(), 0.62);

        producer.write("{bad json}\n");
        QVERIFY(!readReply(producer).value("ok").toBool());
        send(producer, {{"version", 2}, {"op", "list"}});
        QVERIFY(!readReply(producer).value("ok").toBool());
        send(producer, {{"version", 1}, {"op", "list"}});
        QCOMPARE(readReply(producer).value("downloads").toArray().size(), 1);
        QLocalSocket lateWatcher;
        lateWatcher.connectToServer(path);
        QVERIFY(lateWatcher.waitForConnected());
        send(lateWatcher, {{"version", 1}, {"op", "watch"}});
        QCOMPARE(readReply(lateWatcher).value("downloads").toArray().size(), 1);
        send(producer, {{"version", 1}, {"op", "remove"},
                        {"source", "brave:default"}, {"id", "123"}});
        QVERIFY(readReply(producer).value("ok").toBool());
        QCOMPARE(readReply(watcher).value("event").toString(), QString("removed"));
        QCOMPARE(readReply(lateWatcher).value("event").toString(), QString("removed"));
        producer.write(QByteArray(DownloadServer::MaxFrameBytes + 1, 'x'));
        QTRY_COMPARE(producer.state(), QLocalSocket::UnconnectedState);
    }

    void clientLimitKeepsServiceResponsive()
    {
        QTemporaryDir directory(QCoreApplication::applicationDirPath() + "/downloads-test-XXXXXX");
        QVERIFY(directory.isValid());
        DownloadServer server;
        QString error;
        const auto path = directory.path() + "/api.sock";
        QVERIFY(server.listen(path, &error));
        std::vector<std::unique_ptr<QLocalSocket>> clients;
        for (int i = 0; i < DownloadServer::MaxClients; ++i) {
            auto socket = std::make_unique<QLocalSocket>();
            socket->connectToServer(path);
            QVERIFY(socket->waitForConnected());
            send(*socket, {{"version", 1}, {"op", "list"}});
            QVERIFY(readReply(*socket).value("ok").toBool());
            clients.push_back(std::move(socket));
        }
        QLocalSocket extra;
        extra.connectToServer(path);
        QVERIFY(extra.waitForConnected());
        QTRY_COMPARE(extra.state(), QLocalSocket::UnconnectedState);
        send(*clients.front(), {{"version", 1}, {"op", "list"}});
        QVERIFY(readReply(*clients.front()).value("ok").toBool());
    }

    void existingFilesAreNotReplaced()
    {
        QTemporaryDir directory(QCoreApplication::applicationDirPath() + "/downloads-test-XXXXXX");
        QVERIFY(directory.isValid());
        const auto path = directory.path() + "/important.txt";
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("keep this file"), 14);
        file.close();
        DownloadServer server;
        QString error;
        QVERIFY(!server.listen(path, &error));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("keep this file"));
        file.close();
        const auto link = directory.path() + "/link.sock";
        QVERIFY(QFile::link(path, link));
        QVERIFY(!server.listen(link, &error));
        QVERIFY(QFileInfo(link).isSymLink());
    }

    void slowWatcherIsDisconnected()
    {
        QTemporaryDir directory(QCoreApplication::applicationDirPath() + "/downloads-test-XXXXXX");
        QVERIFY(directory.isValid());
        const auto path = directory.path() + "/api.sock";
        DownloadServer server;
        QString error;
        QVERIFY(server.listen(path, &error));
        QLocalSocket watcher;
        watcher.setReadBufferSize(1);
        watcher.connectToServer(path);
        QVERIFY(watcher.waitForConnected());
        send(watcher, {{"version", 1}, {"op", "watch"}});
        QLocalSocket producer;
        producer.connectToServer(path);
        QVERIFY(producer.waitForConnected());
        auto item = download();
        item["filename"] = QString(4096, 'x');
        // Exceed both the kernel socket buffer and the server output allowance.
        for (int i = 0; i < 1600; ++i) {
            item["receivedBytes"] = i;
            send(producer, {{"version", 1}, {"op", "update"}, {"download", item}});
            QVERIFY(readReply(producer).value("ok").toBool());
        }
        // Drain buffered data so the client can observe the server-side close.
        watcher.setReadBufferSize(0);
        QTRY_COMPARE(watcher.state(), QLocalSocket::UnconnectedState);
        send(producer, {{"version", 1}, {"op", "list"}});
        QVERIFY(readReply(producer).value("ok").toBool());
    }
};

QTEST_GUILESS_MAIN(DownloadProgressTest)
#include "DownloadProgressTest.moc"
