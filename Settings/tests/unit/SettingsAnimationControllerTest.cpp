#include "services/animation/SettingsAnimationController.hpp"
#include "services/animation/SettingsTyphonControlClient.hpp"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMutex>
#include <QSignalSpy>
#include <QThread>
#include <QTemporaryDir>
#include <QTest>
#include <QWaitCondition>

#include <sys/stat.h>

#include <utility>

namespace {

QJsonObject animationSnapshot(bool enabled, const QString &preset, double speed,
                              const QJsonObject &overrides = {})
{
    const QJsonArray presets{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("astrea")}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("kde")}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("macos")}},
    };
    const QJsonArray slotCapabilities{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("window.move")},
                    {QStringLiteral("compatibleEffects"),
                     QJsonArray{QStringLiteral("none"), QStringLiteral("geometry.kde"),
                                QStringLiteral("geometry.macos")}}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("window.minimize")},
                    {QStringLiteral("compatibleEffects"),
                     QJsonArray{QStringLiteral("none"), QStringLiteral("minimize.lamp")}}},
    };
    const QJsonArray effects{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("none")},
                    {QStringLiteral("availability"), QStringLiteral("available")}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("geometry.kde")},
                    {QStringLiteral("availability"), QStringLiteral("available")}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("geometry.macos")},
                    {QStringLiteral("availability"), QStringLiteral("available")}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("minimize.lamp")},
                    {QStringLiteral("availability"), QStringLiteral("planned")}},
    };
    return {
        {QStringLiteral("generation"), 4},
        {QStringLiteral("source"), QStringLiteral("persisted")},
        {QStringLiteral("startupOverride"), false},
        {QStringLiteral("config"), QJsonObject{
            {QStringLiteral("enabled"), enabled},
            {QStringLiteral("preset"), preset},
            {QStringLiteral("speed"), speed},
            {QStringLiteral("overrides"), overrides},
        }},
        {QStringLiteral("requested"), QJsonObject{
            {QStringLiteral("window.move"), preset == QStringLiteral("macos")
                                                   ? QStringLiteral("geometry.macos")
                                                   : QStringLiteral("geometry.kde")},
            {QStringLiteral("window.minimize"), preset == QStringLiteral("astrea")
                                                    ? QStringLiteral("minimize.lamp")
                                                    : QStringLiteral("none")},
        }},
        {QStringLiteral("effective"), QJsonObject{
            {QStringLiteral("window.move"), preset == QStringLiteral("macos")
                                                   ? QStringLiteral("geometry.macos")
                                                   : QStringLiteral("geometry.kde")},
            {QStringLiteral("window.minimize"), QStringLiteral("none")},
        }},
        {QStringLiteral("catalog"), QJsonObject{
            {QStringLiteral("presets"), presets},
            {QStringLiteral("slots"), slotCapabilities},
            {QStringLiteral("effects"), effects},
        }},
    };
}

class EnvironmentGuard final {
public:
    EnvironmentGuard(const QByteArray &runtime, const QByteArray &display)
        : m_runtime(qgetenv("XDG_RUNTIME_DIR"))
        , m_display(qgetenv("WAYLAND_DISPLAY"))
        , m_hadRuntime(qEnvironmentVariableIsSet("XDG_RUNTIME_DIR"))
        , m_hadDisplay(qEnvironmentVariableIsSet("WAYLAND_DISPLAY"))
    {
        qputenv("XDG_RUNTIME_DIR", runtime);
        qputenv("WAYLAND_DISPLAY", display);
    }

    ~EnvironmentGuard()
    {
        if (m_hadRuntime)
            qputenv("XDG_RUNTIME_DIR", m_runtime);
        else
            qunsetenv("XDG_RUNTIME_DIR");
        if (m_hadDisplay)
            qputenv("WAYLAND_DISPLAY", m_display);
        else
            qunsetenv("WAYLAND_DISPLAY");
    }

private:
    QByteArray m_runtime;
    QByteArray m_display;
    bool m_hadRuntime = false;
    bool m_hadDisplay = false;
};

class AnimationControlServer final : public QObject {
public:
    explicit AnimationControlServer(const QString &runtimePath, QObject *parent = nullptr)
        : QObject(parent)
    {
        const QString instance = QDir(runtimePath).filePath(QStringLiteral("astrea/typhon/test"));
        QVERIFY(QDir().mkpath(instance));
        QVERIFY(::chmod(QFile::encodeName(runtimePath).constData(), 0700) == 0);
        QVERIFY(::chmod(QFile::encodeName(QDir(runtimePath).filePath(QStringLiteral("astrea"))).constData(), 0700) == 0);
        QVERIFY(::chmod(QFile::encodeName(QDir(runtimePath).filePath(QStringLiteral("astrea/typhon"))).constData(), 0700) == 0);
        QVERIFY(::chmod(QFile::encodeName(instance).constData(), 0700) == 0);
        m_endpoint = QDir(instance).filePath(QStringLiteral("control.sock"));
        m_server.moveToThread(&m_thread);
        connect(&m_thread, &QThread::started, &m_server, [this] {
            m_server.setSocketOptions(QLocalServer::UserAccessOption);
            const bool listening = m_server.listen(m_endpoint)
                && ::chmod(QFile::encodeName(m_endpoint).constData(), 0600) == 0;
            {
                QMutexLocker locker(&m_mutex);
                m_listening = listening;
                m_ready.wakeAll();
            }
        });
        connect(&m_server, &QLocalServer::newConnection, &m_server, [this] {
            auto *socket = m_server.nextPendingConnection();
            connect(socket, &QLocalSocket::readyRead, socket, [this, socket] {
                serveSocket(socket);
            });
        });
        m_snapshot = animationSnapshot(true, QStringLiteral("astrea"), 1.0);
        m_thread.start();
        QMutexLocker locker(&m_mutex);
        if (!m_listening)
            m_ready.wait(&m_mutex, 2000);
        QVERIFY(m_listening);
    }

    ~AnimationControlServer() override
    {
        m_thread.quit();
        m_thread.wait();
        m_server.close();
        QLocalServer::removeServer(m_endpoint);
    }

    void rejectNextSet()
    {
        QMutexLocker locker(&m_mutex);
        m_rejectNextSet = true;
    }
    void sendMalformedResponse()
    {
        QMutexLocker locker(&m_mutex);
        m_malformedResponse = true;
    }
    void setResponseMode(const QByteArray &mode)
    {
        QMutexLocker locker(&m_mutex);
        m_responseMode = mode;
    }
    void delayResponses(bool delay)
    {
        QMutexLocker locker(&m_mutex);
        m_delayResponses = delay;
    }
    void releaseDelayedResponses()
    {
        QMetaObject::invokeMethod(&m_server, [this] {
            QList<QPair<QLocalSocket *, QByteArray>> delayed;
            delayed.swap(m_delayedResponses);
            for (const auto &[socket, response] : delayed) {
                if (socket->state() == QLocalSocket::ConnectedState) {
                    socket->write(response);
                    socket->flush();
                }
            }
        }, Qt::QueuedConnection);
    }
    qsizetype commandCount() const
    {
        QMutexLocker locker(&m_mutex);
        return m_commands.size();
    }
    QJsonObject lastRequest() const
    {
        QMutexLocker locker(&m_mutex);
        return m_lastRequest;
    }

private:
    void serveSocket(QLocalSocket *socket)
    {
        m_request += socket->readAll();
        if (!m_request.contains('\n'))
            return;
        const qsizetype newline = m_request.indexOf('\n');
        const QJsonDocument document = QJsonDocument::fromJson(m_request.left(newline));
        m_request.remove(0, newline + 1);
        if (!document.isObject())
            return;
        const QJsonObject request = document.object();
        QJsonObject response;
        {
            QMutexLocker locker(&m_mutex);
            m_lastRequest = request;
            m_commands.append(request.value(QStringLiteral("command")).toString());
            if (m_malformedResponse) {
                m_malformedResponse = false;
                socket->write(QByteArrayLiteral("not-json\n"));
                socket->flush();
                return;
            }
            if (request.value(QStringLiteral("command")).toString()
                == QStringLiteral("animation.config.set")) {
                const QJsonObject args = request.value(QStringLiteral("args")).toObject();
                const QString requestedPreset = args.value(QStringLiteral("preset")).toString();
                if (m_rejectNextSet || (requestedPreset != QStringLiteral("astrea")
                                         && requestedPreset != QStringLiteral("kde")
                                         && requestedPreset != QStringLiteral("macos"))) {
                    m_rejectNextSet = false;
                    response = makeResponse(request.value(QStringLiteral("id")).toInteger(), false,
                                            {}, QStringLiteral("rejected"));
                } else {
                    m_snapshot = animationSnapshot(
                        args.value(QStringLiteral("enabled")).toBool(), requestedPreset,
                        args.value(QStringLiteral("speed")).toDouble(),
                        args.value(QStringLiteral("overrides")).toObject());
                    response = makeResponse(request.value(QStringLiteral("id")).toInteger(), true,
                                            m_snapshot, {});
                }
            } else {
                response = makeResponse(request.value(QStringLiteral("id")).toInteger(), true,
                                        m_snapshot, {});
            }
        }
        const QByteArray encodedResponse = QJsonDocument(response).toJson(QJsonDocument::Compact)
            + '\n';
        QByteArray responseToSend = encodedResponse;
        bool disconnect = false;
        {
            QMutexLocker locker(&m_mutex);
            const QByteArray responseMode = std::exchange(m_responseMode, {});
            if (responseMode == QByteArrayLiteral("protocol")) {
                response[QStringLiteral("protocol")] = QStringLiteral("wrong.protocol");
                responseToSend = QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n';
            } else if (responseMode == QByteArrayLiteral("version")) {
                response[QStringLiteral("version")] = 2;
                responseToSend = QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n';
            } else if (responseMode == QByteArrayLiteral("id")) {
                response[QStringLiteral("id")] = response.value(QStringLiteral("id")).toInteger() + 1;
                responseToSend = QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n';
            } else if (responseMode == QByteArrayLiteral("missing-ok")) {
                response.remove(QStringLiteral("ok"));
                responseToSend = QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n';
            } else if (responseMode == QByteArrayLiteral("success-no-result")) {
                response.remove(QStringLiteral("result"));
                responseToSend = QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n';
            } else if (responseMode == QByteArrayLiteral("error-no-error")) {
                response[QStringLiteral("ok")] = false;
                response.remove(QStringLiteral("result"));
                response.remove(QStringLiteral("error"));
                responseToSend = QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n';
            } else if (responseMode == QByteArrayLiteral("extra-frame")) {
                responseToSend.append('\n');
            } else if (responseMode == QByteArrayLiteral("oversized")) {
                responseToSend = QByteArray(1024 * 1024 + 1, 'x');
            } else if (responseMode == QByteArrayLiteral("invalid-json")) {
                responseToSend = QByteArrayLiteral("not-json\n");
            } else if (responseMode == QByteArrayLiteral("disconnect")) {
                disconnect = true;
            }
            if (m_delayResponses) {
                m_delayedResponses.append({socket, responseToSend});
                return;
            }
        }
        if (disconnect) {
            socket->disconnectFromServer();
            return;
        }
        socket->write(responseToSend);
        socket->flush();
    }

    static QJsonObject makeResponse(qint64 id, bool ok, const QJsonObject &result,
                                    const QString &message)
    {
        QJsonObject response{{QStringLiteral("protocol"), QStringLiteral("astrea.control")},
                             {QStringLiteral("version"), 1},
                             {QStringLiteral("id"), id},
                             {QStringLiteral("ok"), ok}};
        if (ok)
            response.insert(QStringLiteral("result"), result);
        else
            response.insert(QStringLiteral("error"),
                            QJsonObject{{QStringLiteral("message"), message}});
        return response;
    }

    QLocalServer m_server;
    QThread m_thread;
    mutable QMutex m_mutex;
    QWaitCondition m_ready;
    QString m_endpoint;
    QByteArray m_request;
    QStringList m_commands;
    QJsonObject m_lastRequest;
    QJsonObject m_snapshot;
    bool m_listening = false;
    bool m_rejectNextSet = false;
    bool m_malformedResponse = false;
    QByteArray m_responseMode;
    bool m_delayResponses = false;
    QList<QPair<QLocalSocket *, QByteArray>> m_delayedResponses;
};

} // namespace

class SettingsAnimationControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void controllerUsesAuthoritativeSnapshotsAndRejectsPlannedEffects();
    void clientStartsAndCompletesAsynchronously();
    void clientTimeoutDoesNotBlockTheEventLoop();
    void clientRejectsSecondRequestWhileBusyAndCanBeReused();
    void clientRejectsMalformedResponse();
    void clientRejectsInvalidResponseShapesAndBounds();
    void clientHandlesConnectionFailure();
    void clientRejectsOversizedRequestsImmediately();
    void clientRejectsAmbiguousAndInsecureDiscovery();
    void controllerFoldsPendingSpeedIntoNextMutation();
};

void SettingsAnimationControllerTest::controllerUsesAuthoritativeSnapshotsAndRejectsPlannedEffects()
{
    QTemporaryDir runtime;
    QVERIFY(runtime.isValid());
    EnvironmentGuard environment(runtime.path().toUtf8(), QByteArrayLiteral("test"));
    AnimationControlServer server(runtime.path());

    SettingsAnimationController controller;
    QVERIFY(!controller.available());
    controller.refresh();
    QTRY_VERIFY2(controller.available(), qPrintable(controller.lastError()));
    QCOMPARE(controller.preset(), QStringLiteral("astrea"));
    QCOMPARE(controller.speed(), 1.0);
    QVERIFY(!controller.hasOverrides());
    QCOMPARE(controller.presets().size(), 3);
    QVERIFY(!controller.slotCapabilities().isEmpty());
    const auto slot = [&controller](const QString &id) {
        for (const QVariant &value : controller.slotCapabilities()) {
            const QVariantMap candidate = value.toMap();
            if (candidate.value(QStringLiteral("id")).toString() == id)
                return candidate;
        }
        return QVariantMap{};
    };
    const QVariantMap astreaMinimize = slot(QStringLiteral("window.minimize"));
    QCOMPARE(astreaMinimize.value(QStringLiteral("requested")).toString(),
             QStringLiteral("minimize.lamp"));
    QCOMPARE(astreaMinimize.value(QStringLiteral("effective")).toString(), QStringLiteral("none"));
    QVERIFY(astreaMinimize.value(QStringLiteral("plannedEffects")).toStringList().contains(
        QStringLiteral("minimize.lamp")));

    controller.setEnabled(false);
    QTRY_VERIFY(!controller.busy());
    QVERIFY(!controller.enabled());
    controller.setPreset(QStringLiteral("macos"));
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.preset(), QStringLiteral("macos"));
    const QVariantMap macosMinimize = slot(QStringLiteral("window.minimize"));
    QCOMPARE(macosMinimize.value(QStringLiteral("requested")).toString(), QStringLiteral("none"));
    QCOMPARE(macosMinimize.value(QStringLiteral("effective")).toString(), QStringLiteral("none"));
    QVERIFY(macosMinimize.value(QStringLiteral("plannedEffects")).toStringList().contains(
        QStringLiteral("minimize.lamp")));

    const qsizetype requestsBeforePlanned = server.commandCount();
    controller.setSlotEffect(QStringLiteral("window.minimize"), QStringLiteral("minimize.lamp"));
    QCOMPARE(server.commandCount(), requestsBeforePlanned);
    QVERIFY(!controller.lastError().isEmpty());

    controller.setSpeed(1.25);
    controller.flush();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.speed(), 1.25);

    server.rejectNextSet();
    controller.setPreset(QStringLiteral("kde"));
    QTRY_VERIFY(!controller.busy());
    QVERIFY(controller.available());
    QCOMPARE(controller.preset(), QStringLiteral("macos"));
    QVERIFY(!controller.lastError().isEmpty());
}

void SettingsAnimationControllerTest::clientStartsAndCompletesAsynchronously()
{
    QTemporaryDir runtime;
    QVERIFY(runtime.isValid());
    EnvironmentGuard environment(runtime.path().toUtf8(), QByteArrayLiteral("test"));
    AnimationControlServer server(runtime.path());
    server.delayResponses(true);

    SettingsTyphonControlClient client(nullptr, 500);
    QSignalSpy finished(&client, &SettingsTyphonControlClient::requestFinished);
    QElapsedTimer elapsed;
    elapsed.start();
    QString startError;
    QVERIFY(client.startRequest(QStringLiteral("animation.config.get"), {}, &startError));
    QVERIFY2(elapsed.elapsed() < 100, qPrintable(startError));
    QCOMPARE(finished.count(), 0);

    QTRY_COMPARE(server.commandCount(), 1);
    const QJsonObject request = server.lastRequest();
    QCOMPARE(request.value(QStringLiteral("protocol")).toString(), QStringLiteral("astrea.control"));
    QCOMPARE(request.value(QStringLiteral("version")).toInt(), 1);
    QCOMPARE(request.value(QStringLiteral("id")).toInteger(), 1);
    QCOMPARE(request.value(QStringLiteral("command")).toString(), QStringLiteral("animation.config.get"));
    QVERIFY(request.value(QStringLiteral("args")).isObject());
    server.releaseDelayedResponses();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), true);
    QVERIFY(finished.at(0).at(1).toMap().contains(QStringLiteral("config")));
    QVERIFY(finished.at(0).at(2).toString().isEmpty());
}

void SettingsAnimationControllerTest::clientTimeoutDoesNotBlockTheEventLoop()
{
    QTemporaryDir runtime;
    QVERIFY(runtime.isValid());
    EnvironmentGuard environment(runtime.path().toUtf8(), QByteArrayLiteral("test"));
    AnimationControlServer server(runtime.path());
    server.delayResponses(true);

    SettingsTyphonControlClient client(nullptr, 50);
    QSignalSpy finished(&client, &SettingsTyphonControlClient::requestFinished);
    int marker = 0;
    bool markerObservedWhileBusy = false;
    QTimer::singleShot(0, &client, [&marker, &markerObservedWhileBusy, &client] {
        markerObservedWhileBusy = client.busy();
        ++marker;
    });
    QString startError;
    QVERIFY(client.startRequest(QStringLiteral("animation.config.get"), {}, &startError));
    QTRY_COMPARE(marker, 1);
    QVERIFY(markerObservedWhileBusy);
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), false);
    QVERIFY(finished.at(0).at(2).toString().contains(QStringLiteral("timeout")));
}

void SettingsAnimationControllerTest::clientRejectsSecondRequestWhileBusyAndCanBeReused()
{
    QTemporaryDir runtime;
    QVERIFY(runtime.isValid());
    EnvironmentGuard environment(runtime.path().toUtf8(), QByteArrayLiteral("test"));
    AnimationControlServer server(runtime.path());
    server.delayResponses(true);

    SettingsTyphonControlClient client(nullptr, 500);
    QSignalSpy finished(&client, &SettingsTyphonControlClient::requestFinished);
    QString error;
    QVERIFY(client.startRequest(QStringLiteral("animation.config.get"), {}, &error));
    QVERIFY(!client.startRequest(QStringLiteral("animation.config.get"), {}, &error));
    QVERIFY(error.contains(QStringLiteral("in flight")));
    server.releaseDelayedResponses();
    QTRY_COMPARE(finished.count(), 1);

    server.delayResponses(false);
    QVERIFY(client.startRequest(QStringLiteral("animation.config.get"), {}, &error));
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(finished.at(1).at(0).toBool(), true);
}

void SettingsAnimationControllerTest::clientRejectsMalformedResponse()
{
    QTemporaryDir runtime;
    QVERIFY(runtime.isValid());
    EnvironmentGuard environment(runtime.path().toUtf8(), QByteArrayLiteral("test"));
    AnimationControlServer server(runtime.path());
    server.sendMalformedResponse();

    SettingsTyphonControlClient client;
    QSignalSpy finished(&client, &SettingsTyphonControlClient::requestFinished);
    QString error;
    QVERIFY(client.startRequest(QStringLiteral("animation.config.get"), {}, &error));
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.at(0).at(2).toString().contains(QStringLiteral("JSON")));
}

void SettingsAnimationControllerTest::clientRejectsInvalidResponseShapesAndBounds()
{
    QTemporaryDir runtime;
    QVERIFY(runtime.isValid());
    EnvironmentGuard environment(runtime.path().toUtf8(), QByteArrayLiteral("test"));
    AnimationControlServer server(runtime.path());
    SettingsTyphonControlClient client(nullptr, 500);
    QSignalSpy finished(&client, &SettingsTyphonControlClient::requestFinished);
    const QList<QByteArray> modes{
        QByteArrayLiteral("invalid-json"), QByteArrayLiteral("protocol"),
        QByteArrayLiteral("version"), QByteArrayLiteral("id"),
        QByteArrayLiteral("missing-ok"), QByteArrayLiteral("success-no-result"),
        QByteArrayLiteral("error-no-error"), QByteArrayLiteral("extra-frame"),
        QByteArrayLiteral("oversized"),
    };
    for (qsizetype index = 0; index < modes.size(); ++index) {
        server.setResponseMode(modes.at(index));
        QString startError;
        QVERIFY2(client.startRequest(QStringLiteral("animation.config.get"), {}, &startError),
                 qPrintable(startError));
        QTRY_COMPARE(finished.count(), index + 1);
        QCOMPARE(finished.at(index).at(0).toBool(), false);
        QVERIFY(!finished.at(index).at(2).toString().isEmpty());
    }
}

void SettingsAnimationControllerTest::clientHandlesConnectionFailure()
{
    QTemporaryDir runtime;
    QVERIFY(runtime.isValid());
    EnvironmentGuard environment(runtime.path().toUtf8(), QByteArrayLiteral("test"));
    AnimationControlServer server(runtime.path());
    server.setResponseMode(QByteArrayLiteral("disconnect"));
    SettingsTyphonControlClient client(nullptr, 500);
    QSignalSpy finished(&client, &SettingsTyphonControlClient::requestFinished);
    QString startError;
    QVERIFY(client.startRequest(QStringLiteral("animation.config.get"), {}, &startError));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), false);
    QVERIFY(finished.at(0).at(2).toString().contains(QStringLiteral("disconnected"))
            || finished.at(0).at(2).toString().contains(QStringLiteral("transport")));
}

void SettingsAnimationControllerTest::clientRejectsOversizedRequestsImmediately()
{
    QTemporaryDir runtime;
    QVERIFY(runtime.isValid());
    EnvironmentGuard environment(runtime.path().toUtf8(), QByteArrayLiteral("test"));
    AnimationControlServer server(runtime.path());
    SettingsTyphonControlClient client;
    const QVariantMap arguments{{QStringLiteral("payload"), QString(70 * 1024, QLatin1Char('x'))}};
    QString error;
    QVERIFY(!client.startRequest(QStringLiteral("animation.config.get"), arguments, &error));
    QVERIFY(error.contains(QStringLiteral("64 KiB")));
    QCOMPARE(server.commandCount(), 0);
}

void SettingsAnimationControllerTest::clientRejectsAmbiguousAndInsecureDiscovery()
{
    {
        QTemporaryDir runtime;
        QVERIFY(runtime.isValid());
        EnvironmentGuard environment(runtime.path().toUtf8(), QByteArrayLiteral("test"));
        AnimationControlServer server(runtime.path());
        qputenv("WAYLAND_DISPLAY", QByteArrayLiteral("missing"));
        const QString instance = QDir(runtime.path()).filePath(QStringLiteral("astrea/typhon/other"));
        QVERIFY(QDir().mkpath(instance));
        QVERIFY(::chmod(QFile::encodeName(instance).constData(), 0700) == 0);
        const QString endpoint = QDir(instance).filePath(QStringLiteral("control.sock"));
        QLocalServer other;
        other.setSocketOptions(QLocalServer::UserAccessOption);
        QVERIFY(other.listen(endpoint));
        QVERIFY(::chmod(QFile::encodeName(endpoint).constData(), 0600) == 0);
        SettingsTyphonControlClient client;
        QString error;
        QVERIFY(!client.startRequest(QStringLiteral("animation.config.get"), {}, &error));
        QVERIFY(error.contains(QStringLiteral("multiple")));
    }

    QTemporaryDir runtime;
    QVERIFY(runtime.isValid());
    EnvironmentGuard environment(runtime.path().toUtf8(), QByteArrayLiteral("missing"));
    const QString root = QDir(runtime.path()).filePath(QStringLiteral("astrea/typhon"));
    QVERIFY(QDir().mkpath(root));
    QVERIFY(::chmod(QFile::encodeName(runtime.path()).constData(), 0700) == 0);
    QVERIFY(::chmod(QFile::encodeName(QDir(runtime.path()).filePath(QStringLiteral("astrea"))).constData(), 0700) == 0);
    QVERIFY(::chmod(QFile::encodeName(root).constData(), 0700) == 0);
    const QString instance = QDir(root).filePath(QStringLiteral("insecure"));
    QVERIFY(QDir().mkpath(instance));
    QVERIFY(::chmod(QFile::encodeName(instance).constData(), 0755) == 0);
    const QString endpoint = QDir(instance).filePath(QStringLiteral("control.sock"));
    QLocalServer insecure;
    insecure.setSocketOptions(QLocalServer::UserAccessOption);
    QVERIFY(insecure.listen(endpoint));
    QVERIFY(::chmod(QFile::encodeName(endpoint).constData(), 0600) == 0);
    SettingsTyphonControlClient client;
    QString error;
    QVERIFY(!client.startRequest(QStringLiteral("animation.config.get"), {}, &error));
    QVERIFY(error.contains(QStringLiteral("no secure")));
}

void SettingsAnimationControllerTest::controllerFoldsPendingSpeedIntoNextMutation()
{
    QTemporaryDir runtime;
    QVERIFY(runtime.isValid());
    EnvironmentGuard environment(runtime.path().toUtf8(), QByteArrayLiteral("test"));
    AnimationControlServer server(runtime.path());

    SettingsAnimationController controller;
    controller.refresh();
    QTRY_VERIFY(controller.available());
    const qsizetype before = server.commandCount();

    controller.setSpeed(1.5);
    controller.setPreset(QStringLiteral("macos"));
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(server.commandCount(), before + 1);
    QCOMPARE(controller.speed(), 1.5);
    QCOMPARE(controller.preset(), QStringLiteral("macos"));
}

QTEST_GUILESS_MAIN(SettingsAnimationControllerTest)
#include "SettingsAnimationControllerTest.moc"
