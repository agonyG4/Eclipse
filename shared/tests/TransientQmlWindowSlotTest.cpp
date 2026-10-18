#include "platform/wayland/TransientQmlWindowSlot.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <thread>

class TransientQmlWindowSlotTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void startsUncreatedAndReusesTheLiveWindow();
    void destroysAtTheQueuedGuiBoundaryAndBalancesCounters();
    void aReopenInvalidatesQueuedDestruction();
    void componentFailureLeavesTheSlotUncreated();
    void configurationFailureDestroysThePartialWindow();
    void visibleDemandCannotMapBeforeConfiguration();
    void offThreadDestructionIsDeliveredOnTheOwnerThread();

private:
    QUrl createQmlFile(const QString &body);

    QTemporaryDir m_directory;
    QQmlEngine m_engine;
    QScreen *m_screen = nullptr;
    int m_fileCounter = 0;
};

void TransientQmlWindowSlotTest::initTestCase()
{
    QVERIFY(m_directory.isValid());
    m_screen = QGuiApplication::primaryScreen();
    QVERIFY(m_screen);
}

QUrl TransientQmlWindowSlotTest::createQmlFile(const QString &body)
{
    const QString relativeDirectory = QStringLiteral("case-%1").arg(++m_fileCounter);
    if (!QDir().mkpath(m_directory.filePath(relativeDirectory)))
        return {};
    const QString path = m_directory.filePath(relativeDirectory + QStringLiteral("/Window.qml"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    file.write(body.toUtf8());
    file.close();
    return QUrl::fromLocalFile(path);
}

void TransientQmlWindowSlotTest::startsUncreatedAndReusesTheLiveWindow()
{
    const QUrl url = createQmlFile(QStringLiteral(
        "import QtQuick\nimport QtQuick.Window\n"
        "Window { property bool surfaceReady: false; property bool demand: false; "
        "visible: surfaceReady && demand; width: 64; height: 48 }"));
    QVERIFY(url.isValid());

    TransientQmlWindowSlot slot(QStringLiteral("test"));
    QVERIFY(!slot.created());
    QVERIFY(slot.window() == nullptr);
    QCOMPARE(slot.createCount(), quint64(0));
    QCOMPARE(slot.destroyCount(), quint64(0));
    QCOMPARE(slot.metaObject()->indexOfProperty("featureState"), -1);

    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = url;
    request.screen = m_screen;
    request.initialProperties.insert(QStringLiteral("demand"), true);
    request.requireSurfaceReadyGate = true;
    request.configure = [](QQuickWindow *window, QString *) {
        return !window->isVisible() && !window->isPersistentGraphics()
            && !window->isPersistentSceneGraph();
    };

    QQuickWindow *first = slot.ensureCreated(&m_engine, request);
    QVERIFY(first);
    QVERIFY(first->isVisible());
    QCOMPARE(slot.window(), first);
    QCOMPARE(slot.createCount(), quint64(1));
    QCOMPARE(slot.destroyCount(), quint64(0));
    QCOMPARE(slot.ensureCreated(&m_engine, request), first);
    QCOMPARE(slot.createCount(), quint64(1));
}

void TransientQmlWindowSlotTest::destroysAtTheQueuedGuiBoundaryAndBalancesCounters()
{
    const QUrl url = createQmlFile(QStringLiteral(
        "import QtQuick\nimport QtQuick.Window\n"
        "Window { visible: false; width: 64; height: 48 }"));
    TransientQmlWindowSlot slot(QStringLiteral("test"));
    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = url;
    request.screen = m_screen;
    QString error;
    QPointer<QQuickWindow> window = slot.ensureCreated(&m_engine, request, &error);
    QVERIFY2(window, qPrintable(error));

    QSignalSpy destroyedSpy(&slot, &TransientQmlWindowSlot::windowDestroyed);
    slot.destroyWhenSafe();
    QTRY_VERIFY(window.isNull());
    QCOMPARE(destroyedSpy.count(), 1);
    QVERIFY(!slot.created());
    QCOMPARE(slot.createCount(), quint64(1));
    QCOMPARE(slot.destroyCount(), quint64(1));
    const QVariantMap snapshot = slot.residencySnapshot();
    QCOMPARE(snapshot.value(QStringLiteral("width")).toInt(), 0);
    QCOMPARE(snapshot.value(QStringLiteral("height")).toInt(), 0);
    QCOMPARE(snapshot.value(QStringLiteral("visible")).toBool(), false);
}

void TransientQmlWindowSlotTest::aReopenInvalidatesQueuedDestruction()
{
    const QUrl url = createQmlFile(QStringLiteral(
        "import QtQuick\nimport QtQuick.Window\n"
        "Window { visible: false; width: 64; height: 48 }"));
    TransientQmlWindowSlot slot(QStringLiteral("test"));
    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = url;
    request.screen = m_screen;
    QString error;
    QPointer<QQuickWindow> original = slot.ensureCreated(&m_engine, request, &error);
    QVERIFY2(original, qPrintable(error));

    slot.destroyWhenSafe();
    QVERIFY(slot.residencySnapshot().value(QStringLiteral("destroyPending")).toBool());
    slot.cancelPendingDestruction();
    QVERIFY(!slot.residencySnapshot().value(QStringLiteral("destroyPending")).toBool());
    QCoreApplication::processEvents();
    QVERIFY(original);
    QCOMPARE(slot.createCount(), quint64(1));
    QCOMPARE(slot.destroyCount(), quint64(0));
    slot.destroyNow();
    QCOMPARE(slot.destroyCount(), quint64(1));
}

void TransientQmlWindowSlotTest::componentFailureLeavesTheSlotUncreated()
{
    TransientQmlWindowSlot slot(QStringLiteral("test"));
    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = QUrl::fromLocalFile(m_directory.filePath(QStringLiteral("missing.qml")));
    request.screen = m_screen;
    QString error;
    QVERIFY(!slot.ensureCreated(&m_engine, request, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!slot.created());
    QCOMPARE(slot.createCount(), quint64(0));
    QCOMPARE(slot.destroyCount(), quint64(0));
}

void TransientQmlWindowSlotTest::configurationFailureDestroysThePartialWindow()
{
    const QUrl url = createQmlFile(QStringLiteral(
        "import QtQuick\nimport QtQuick.Window\n"
        "Window { property bool surfaceReady: false; property bool demand: true; "
        "visible: surfaceReady && demand; width: 64; height: 48 }"));
    TransientQmlWindowSlot slot(QStringLiteral("test"));
    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = url;
    request.screen = m_screen;
    request.requireSurfaceReadyGate = true;
    request.configure = [](QQuickWindow *window, QString *error) {
        if (window->isVisible())
            return false;
        if (error)
            *error = QStringLiteral("injected layer-shell configuration failure");
        return false;
    };
    QString error;
    QVERIFY(!slot.ensureCreated(&m_engine, request, &error));
    QVERIFY(error.contains(QStringLiteral("injected")));
    QVERIFY(!slot.created());
    QCOMPARE(slot.createCount(), quint64(1));
    QCOMPARE(slot.destroyCount(), quint64(1));
}

void TransientQmlWindowSlotTest::visibleDemandCannotMapBeforeConfiguration()
{
    const QUrl url = createQmlFile(QStringLiteral(
        "import QtQuick\nimport QtQuick.Window\n"
        "Window { property bool surfaceReady: false; property bool demand: true; "
        "visible: surfaceReady && demand; width: 64; height: 48 }"));
    TransientQmlWindowSlot slot(QStringLiteral("test"));
    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = url;
    request.screen = m_screen;
    request.requireSurfaceReadyGate = true;
    bool configured = false;
    request.configure = [&configured](QQuickWindow *window, QString *) {
        configured = true;
        return !window->isVisible();
    };
    request.install = [&configured](QQuickWindow *window, QString *) {
        return configured && !window->isVisible();
    };

    QString error;
    QQuickWindow *window = slot.ensureCreated(&m_engine, request, &error);
    QVERIFY2(window, qPrintable(error));
    QVERIFY(configured);
    QVERIFY(window->isVisible());
    slot.destroyNow();
}

void TransientQmlWindowSlotTest::offThreadDestructionIsDeliveredOnTheOwnerThread()
{
    const QUrl url = createQmlFile(QStringLiteral(
        "import QtQuick\nimport QtQuick.Window\n"
        "Window { visible: false; width: 64; height: 48 }"));
    TransientQmlWindowSlot slot(QStringLiteral("test"));
    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = url;
    request.screen = m_screen;
    QString error;
    QVERIFY2(slot.ensureCreated(&m_engine, request, &error), qPrintable(error));
    QThread *destroyThread = nullptr;
    connect(&slot, &TransientQmlWindowSlot::windowDestroyed, &slot,
            [&destroyThread] { destroyThread = QThread::currentThread(); });

    std::thread requester([&slot] { slot.destroyWhenSafe(); });
    requester.join();
    QTRY_VERIFY(!slot.created());
    QCOMPARE(destroyThread, QGuiApplication::instance()->thread());
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication application(argc, argv);
    TransientQmlWindowSlotTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TransientQmlWindowSlotTest.moc"
