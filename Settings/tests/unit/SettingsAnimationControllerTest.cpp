#include "services/animation/SettingsAnimationController.hpp"
#include "services/animation/SettingsTyphonControlClient.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMutex>
#include <QThread>
#include <QTemporaryDir>
#include <QTest>
#include <QWaitCondition>

#include <sys/stat.h>

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
            {QStringLiteral("window.minimize"), QStringLiteral("minimize.lamp")},
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
    qsizetype commandCount() const
    {
        QMutexLocker locker(&m_mutex);
        return m_commands.size();
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
        socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n');
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
    QJsonObject m_snapshot;
    bool m_listening = false;
    bool m_rejectNextSet = false;
    bool m_malformedResponse = false;
};

} // namespace

class SettingsAnimationControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void controllerUsesAuthoritativeSnapshotsAndRejectsPlannedEffects();
    void clientRejectsMalformedResponse();
};

void SettingsAnimationControllerTest::controllerUsesAuthoritativeSnapshotsAndRejectsPlannedEffects()
{
    QTemporaryDir runtime;
    QVERIFY(runtime.isValid());
    EnvironmentGuard environment(runtime.path().toUtf8(), QByteArrayLiteral("test"));
    AnimationControlServer server(runtime.path());

    SettingsAnimationController controller;
    QVERIFY2(controller.available(), qPrintable(controller.lastError()));
    QCOMPARE(controller.preset(), QStringLiteral("astrea"));
    QCOMPARE(controller.speed(), 1.0);
    QVERIFY(!controller.hasOverrides());
    QCOMPARE(controller.presets().size(), 3);
    QVERIFY(!controller.slotCapabilities().isEmpty());

    controller.setEnabled(false);
    QVERIFY(!controller.enabled());
    controller.setPreset(QStringLiteral("macos"));
    QCOMPARE(controller.preset(), QStringLiteral("macos"));

    const qsizetype requestsBeforePlanned = server.commandCount();
    controller.setSlotEffect(QStringLiteral("window.minimize"), QStringLiteral("minimize.lamp"));
    QCOMPARE(server.commandCount(), requestsBeforePlanned);
    QVERIFY(!controller.lastError().isEmpty());

    controller.setSpeed(1.25);
    controller.flush();
    QCOMPARE(controller.speed(), 1.25);

    server.rejectNextSet();
    controller.setPreset(QStringLiteral("kde"));
    QCOMPARE(controller.preset(), QStringLiteral("macos"));
    QVERIFY(!controller.lastError().isEmpty());
}

void SettingsAnimationControllerTest::clientRejectsMalformedResponse()
{
    QTemporaryDir runtime;
    QVERIFY(runtime.isValid());
    EnvironmentGuard environment(runtime.path().toUtf8(), QByteArrayLiteral("test"));
    AnimationControlServer server(runtime.path());
    server.sendMalformedResponse();

    SettingsTyphonControlClient client;
    QVariantMap result;
    QString error;
    QVERIFY(!client.request(QStringLiteral("animation.config.get"), {}, &result, &error));
    QVERIFY2(error.contains(QStringLiteral("JSON")), qPrintable(error));
}

QTEST_GUILESS_MAIN(SettingsAnimationControllerTest)
#include "SettingsAnimationControllerTest.moc"
