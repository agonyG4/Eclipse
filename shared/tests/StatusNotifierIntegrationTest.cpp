#include "statusnotifier/DBusMenuModel.hpp"
#include "statusnotifier/StatusNotifierIconStore.hpp"
#include "statusnotifier/StatusNotifierItemModel.hpp"
#include "statusnotifier/StatusNotifierService.hpp"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QElapsedTimer>
#include <QProcess>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

using namespace Astrea::StatusNotifier;

class StatusNotifierIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void realSessionBusRegistrationActionsAndMenuLifecycle();
    void watcherAliasesRemainObservedAcrossAuthoritySwitches();

private:
    static bool waitForItemCount(StatusNotifierService &service, int expected, int timeoutMs);
    static QString diagnostics(QProcess &fixture);
};

bool StatusNotifierIntegrationTest::waitForItemCount(StatusNotifierService &service,
                                                      int expected, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (service.itemCount() != expected && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(5);
    }
    return service.itemCount() == expected;
}

QString StatusNotifierIntegrationTest::diagnostics(QProcess &fixture)
{
    const QDBusConnection bus = QDBusConnection::sessionBus();
    QString result = QStringLiteral("fixture state=%1 exitCode=%2 error=%3\nstdout=%4\nstderr=%5")
        .arg(fixture.state())
        .arg(fixture.exitCode())
        .arg(fixture.errorString(), QString::fromLocal8Bit(fixture.readAllStandardOutput()),
             QString::fromLocal8Bit(fixture.readAllStandardError()));
    if (bus.isConnected()) {
        if (const auto *iface = bus.interface())
            result += QStringLiteral("\nListNames=%1")
                .arg(iface->registeredServiceNames().value().join(','));
        const QDBusMessage introspection = bus.call(
            QDBusMessage::createMethodCall(
                QStringLiteral("org.astrea.tests.StatusNotifierFixture"),
                QStringLiteral("/org/astrea/tests/StatusNotifierFixture"),
                QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect")));
        result += QStringLiteral("\nfixture introspection type=%1 error=%2")
            .arg(introspection.type()).arg(introspection.errorMessage());
        QDBusInterface watcher(QStringLiteral("org.freedesktop.StatusNotifierWatcher"),
                               QStringLiteral("/StatusNotifierWatcher"),
                               QStringLiteral("org.freedesktop.StatusNotifierWatcher"), bus);
        result += QStringLiteral("\nwatcher items=%1 host=%2")
            .arg(watcher.property("RegisteredStatusNotifierItems").toStringList().join(','))
            .arg(watcher.property("IsStatusNotifierHostRegistered").toBool());
        QDBusInterface control(QStringLiteral("org.astrea.tests.StatusNotifierFixture"),
                               QStringLiteral("/org/astrea/tests/StatusNotifierFixture"),
                               QStringLiteral("org.astrea.tests.StatusNotifierFixture"), bus);
        result += QStringLiteral("\nfixture lastError=%1 ready=%2")
            .arg(control.call(QStringLiteral("Status")).arguments().value(0).toString())
            .arg(control.property("Ready").toBool());
        QDBusInterface item(QStringLiteral("org.astrea.tests.StatusNotifierFixtureItem"),
                            QStringLiteral("/StatusNotifierItem"),
                            QStringLiteral("org.freedesktop.DBus.Properties"), bus);
        const QDBusMessage itemReply = item.call(
            QStringLiteral("GetAll"), QStringLiteral("org.freedesktop.StatusNotifierItem"));
        result += QStringLiteral("\nitem GetAll type=%1 error=%2 args=%3")
            .arg(itemReply.type()).arg(itemReply.errorMessage()).arg(itemReply.arguments().size());
        const QString fixtureOwner = bus.interface()
            ? bus.interface()->serviceOwner(QStringLiteral("org.astrea.tests.StatusNotifierFixture"))
                  .value()
            : QString();
        QDBusInterface pathItem(fixtureOwner, QStringLiteral("/org/test/Tray"),
                                QStringLiteral("org.freedesktop.DBus.Properties"), bus);
        const QDBusMessage pathReply = pathItem.call(
            QStringLiteral("GetAll"), QStringLiteral("org.freedesktop.StatusNotifierItem"));
        result += QStringLiteral("\npath owner=%1 GetAll type=%2 error=%3 args=%4")
            .arg(fixtureOwner).arg(pathReply.type()).arg(pathReply.errorMessage())
            .arg(pathReply.arguments().size());
    }
    return result;
}

void StatusNotifierIntegrationTest::realSessionBusRegistrationActionsAndMenuLifecycle()
{
    QVERIFY2(!qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS").isEmpty(),
             "the mandatory integration test requires an isolated session bus");

    StatusNotifierService service;
    service.start();
    QTRY_VERIFY_WITH_TIMEOUT(service.watcherMode() == WatcherMode::Owned, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(service.hostRegistered(), 2000);
    QVERIFY(!service.hostServiceName().isEmpty());
    QVERIFY(service.hostServiceName().startsWith(
        QStringLiteral("org.freedesktop.StatusNotifierHost-")));
    const QString hostServiceName = service.hostServiceName();
    QVERIFY(QDBusConnection::sessionBus().interface()->isServiceRegistered(hostServiceName));

    QDBusInterface watcherIntrospection(QStringLiteral("org.freedesktop.StatusNotifierWatcher"),
                                        QStringLiteral("/StatusNotifierWatcher"),
                                        QStringLiteral("org.freedesktop.DBus.Introspectable"),
                                        QDBusConnection::sessionBus());
    const QDBusMessage watcherXmlReply = watcherIntrospection.call(QStringLiteral("Introspect"));
    QVERIFY(watcherXmlReply.type() != QDBusMessage::ErrorMessage);
    const QString watcherXml = watcherXmlReply.arguments().value(0).toString();
    const QRegularExpression hostSignal(
        QStringLiteral("<signal name=\\\"StatusNotifierHostRegistered\\\">(.*?)</signal>"),
        QRegularExpression::DotMatchesEverythingOption);
    const auto hostMatch = hostSignal.match(watcherXml);
    QVERIFY(hostMatch.hasMatch());
    QVERIFY(!hostMatch.captured(1).contains(QStringLiteral("<arg")));

    const QString fixturePath = QStringLiteral(ASTREA_STATUSNOTIFIER_FIXTURE_PATH);
    QProcess fixture;
    fixture.setProcessChannelMode(QProcess::SeparateChannels);
    fixture.start(fixturePath);
    QVERIFY2(fixture.waitForStarted(2000), qPrintable(fixture.errorString()));

    QDBusConnectionInterface *busInterface = QDBusConnection::sessionBus().interface();
    QVERIFY(busInterface);
    QElapsedTimer readyTimer;
    readyTimer.start();
    while (!busInterface->isServiceRegistered(
               QStringLiteral("org.astrea.tests.StatusNotifierFixture"))
           && readyTimer.elapsed() < 3000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(5);
    }
    QVERIFY2(busInterface->isServiceRegistered(
                 QStringLiteral("org.astrea.tests.StatusNotifierFixture")),
             qPrintable(diagnostics(fixture)));

    QDBusInterface control(QStringLiteral("org.astrea.tests.StatusNotifierFixture"),
                           QStringLiteral("/org/astrea/tests/StatusNotifierFixture"),
                           QStringLiteral("org.astrea.tests.StatusNotifierFixture"),
                           QDBusConnection::sessionBus());
    QVERIFY2(control.isValid(), qPrintable(diagnostics(fixture)));
    QDBusInterface introspect(QStringLiteral("org.astrea.tests.StatusNotifierFixture"),
                              QStringLiteral("/org/astrea/tests/StatusNotifierFixture"),
                              QStringLiteral("org.freedesktop.DBus.Introspectable"),
                              QDBusConnection::sessionBus());
    const QDBusMessage introspection = introspect.call(QStringLiteral("Introspect"));
    QVERIFY2(introspection.type() != QDBusMessage::ErrorMessage
                 && !introspection.arguments().isEmpty(),
             qPrintable(diagnostics(fixture)));
    QVERIFY(control.property("Ready").toBool());

    QVERIFY2(control.call(QStringLiteral("RegisterServiceOnly")).type()
                 != QDBusMessage::ErrorMessage,
             qPrintable(diagnostics(fixture)));
    if (!waitForItemCount(service, 1, 5000)) {
        qWarning() << "service health" << service.healthJson()
                   << "fixture" << diagnostics(fixture);
        QVERIFY2(false, qPrintable(diagnostics(fixture)));
        return;
    }

    QString key = service.typedItemModel()->keys().constFirst();
    ItemSnapshot snapshot = service.typedItemModel()->item(key);
    QCOMPARE(snapshot.address.service,
             QStringLiteral("org.astrea.tests.StatusNotifierFixtureItem"));
    QCOMPARE(snapshot.address.objectPath, QStringLiteral("/StatusNotifierItem"));
    QCOMPARE(snapshot.tooltipTitle, QStringLiteral("Exact tooltip title"));
    QCOMPARE(snapshot.tooltipDescription, QStringLiteral("Exact tooltip body"));
    QVERIFY2(!snapshot.menuPath.isEmpty(), qPrintable(snapshot.menuPath));
    QCOMPARE(service.iconStore()->image(key).pixel(0, 0), qRgba(0x33, 0x66, 0xcc, 0xff));

    QDBusInterface item(snapshot.address.service, snapshot.address.objectPath,
                        QStringLiteral("org.freedesktop.StatusNotifierItem"),
                        QDBusConnection::sessionBus());
    service.activate(key, 1, 2);
    service.secondaryActivate(key, 3, 4);
    service.contextMenu(key, 5, 6);
    service.scroll(key, 120, QStringLiteral("vertical"));
    QTRY_COMPARE_WITH_TIMEOUT(item.property("ActivateCalls").toInt(), 1, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(item.property("SecondaryActivateCalls").toInt(), 1, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(item.property("ContextMenuCalls").toInt(), 1, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(item.property("ScrollCalls").toInt(), 1, 2000);
    QCOMPARE(item.property("LastX").toInt(), 5);
    QCOMPARE(item.property("LastY").toInt(), 6);
    QCOMPARE(item.property("LastDelta").toInt(), 120);
    QCOMPARE(item.property("LastOrientation").toString(), QStringLiteral("vertical"));

    QDBusInterface menu(snapshot.address.service, QStringLiteral("/org/test/MenuA"),
                        QStringLiteral("com.canonical.dbusmenu"), QDBusConnection::sessionBus());
    service.prepareMenuForPresentation(key);
    QTRY_VERIFY_WITH_TIMEOUT(service.menuModelForItem(key) != nullptr, 2000);
    auto *model = qobject_cast<DBusMenuModel *>(service.menuModelForItem(key));
    QVERIFY(model);
    auto *menuClient = qobject_cast<DBusMenuClient *>(model->parent());
    QVERIFY(menuClient);
    QSignalSpy menuIdentityChanges(&service, &StatusNotifierService::menuClientChanged);
    QVERIFY(control.call(QStringLiteral("SetTitle"), QStringLiteral("Presentation update"))
                .type() != QDBusMessage::ErrorMessage);
    QTRY_COMPARE_WITH_TIMEOUT(service.typedItemModel()->item(key).title,
                              QStringLiteral("Presentation update"), 3000);
    QCOMPARE(menuIdentityChanges.count(), 0);
    QCOMPARE(service.menuModelForItem(key), model);
    QCOMPARE(qobject_cast<DBusMenuClient *>(model->parent()), menuClient);
    QVERIFY(control.call(QStringLiteral("SetIconColor"), 0xaa, 0xbb, 0xcc).type()
                != QDBusMessage::ErrorMessage);
    QTRY_COMPARE_WITH_TIMEOUT(service.iconStore()->image(key).pixel(0, 0),
                              qRgba(0xaa, 0xbb, 0xcc, 0xff), 3000);
    QCOMPARE(menuIdentityChanges.count(), 0);
    QCOMPARE(service.menuModelForItem(key), model);
    QCOMPARE(qobject_cast<DBusMenuClient *>(model->parent()), menuClient);
    QElapsedTimer menuTimer;
    menuTimer.start();
    while (model->rowCount() != 2 && menuTimer.elapsed() < 4000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(5);
    }
    if (model->rowCount() != 2)
        qWarning() << "menu state" << static_cast<int>(menuClient->state())
                   << "revision" << menuClient->revision()
                   << "directGetLayout" << menu.call(QStringLiteral("GetLayout"), 0, -1,
                                                       QStringList{}).errorMessage();
    QCOMPARE(model->rowCount(), 2);
    QCOMPARE(model->data(model->index(0, 0), DBusMenuModel::LabelRole).toString(),
             QStringLiteral("Tools"));
    QVERIFY(model->data(model->index(1, 0), DBusMenuModel::SeparatorRole).toBool());

    QVERIFY(control.call(QStringLiteral("SetLazySubmenu"), true).type()
                != QDBusMessage::ErrorMessage);
    service.prepareMenuForPresentation(key);
    QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(), 2, 3000);
    QVERIFY(model->data(model->index(0, 0), DBusMenuModel::HasChildrenRole).toBool());
    QTRY_VERIFY_WITH_TIMEOUT(model->childModel(10) == nullptr, 3000);
    menuClient->aboutToShow(10);
    QTRY_VERIFY_WITH_TIMEOUT(model->childModel(10) != nullptr, 3000);

    const int rootShows = menu.property("RootAboutToShowCount").toInt();
    QVERIFY(rootShows >= 1);
    service.prepareMenuForPresentation(key);
    QTRY_COMPARE_WITH_TIMEOUT(menu.property("RootAboutToShowCount").toInt(), rootShows + 1,
                              2000);

    menuClient->aboutToShow(10);
    QTRY_COMPARE_WITH_TIMEOUT(menu.property("LastAboutToShowNode").toInt(), 10, 2000);
    auto *child = qobject_cast<DBusMenuModel *>(model->childModel(10));
    QVERIFY(child);
    QTRY_COMPARE_WITH_TIMEOUT(child->rowCount(), 2, 3000);
    menuClient->aboutToShow(20);
    QTRY_COMPARE_WITH_TIMEOUT(menu.property("LastAboutToShowNode").toInt(), 20, 2000);
    auto *nested = qobject_cast<DBusMenuModel *>(child->childModel(20));
    QVERIFY(nested);
    QTRY_COMPARE_WITH_TIMEOUT(nested->rowCount(), 1, 3000);
    nested->activate(30);
    QTRY_COMPARE_WITH_TIMEOUT(menu.property("EventCount").toInt(), 1, 2000);

    QVERIFY(menu.call(QStringLiteral("EmitPropertyUpdate"), QStringLiteral("Live label"))
                .type() != QDBusMessage::ErrorMessage);
    QTRY_COMPARE_WITH_TIMEOUT(child->data(child->index(0, 0), DBusMenuModel::LabelRole).toString(),
                              QStringLiteral("Live label"), 3000);
    QVERIFY(menu.call(QStringLiteral("EmitRemovedProperty")).type() != QDBusMessage::ErrorMessage);
    QTRY_COMPARE_WITH_TIMEOUT(child->data(child->index(0, 0), DBusMenuModel::LabelRole).toString(),
                              QString(), 3000);

    QVERIFY(menu.call(QStringLiteral("SetEmptyMenu"), true).type() != QDBusMessage::ErrorMessage);
    QTRY_COMPARE_WITH_TIMEOUT(static_cast<int>(menuClient->state()),
                              static_cast<int>(DBusMenuLifecycleState::Empty), 3000);
    QVERIFY(menuClient->rootModel()->rowCount() == 0);

    menuIdentityChanges.clear();
    QVERIFY(control.call(QStringLiteral("SetMenuPath"), QStringLiteral("/org/test/MenuB"))
                .type() != QDBusMessage::ErrorMessage);
    QTRY_COMPARE_WITH_TIMEOUT(service.typedItemModel()->item(key).menuPath,
                              QStringLiteral("/org/test/MenuB"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(service.menuClientCount(), 1, 3000);
    QCOMPARE(menuIdentityChanges.count(), 1);
    QVERIFY(service.menuModelForItem(key) != model);

    QVERIFY(control.call(QStringLiteral("SetMenuPath"), QString()).type()
            != QDBusMessage::ErrorMessage);
    QTRY_COMPARE_WITH_TIMEOUT(service.typedItemModel()->item(key).menuPath,
                              QString(), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(service.menuClientCount(), 0, 3000);
    QCOMPARE(menuIdentityChanges.count(), 2);
    QVERIFY(control.call(QStringLiteral("SetMenuPath"), QStringLiteral("/org/test/MenuA"))
                .type() != QDBusMessage::ErrorMessage);
    QTRY_COMPARE_WITH_TIMEOUT(service.menuClientCount(), 1, 3000);
    QCOMPARE(menuIdentityChanges.count(), 3);

    QVERIFY2(control.call(QStringLiteral("UnregisterServiceOnly")).type()
                 != QDBusMessage::ErrorMessage,
             qPrintable(diagnostics(fixture)));
    QVERIFY2(waitForItemCount(service, 0, 4000), qPrintable(diagnostics(fixture)));

    QVERIFY2(control.call(QStringLiteral("RegisterPathOnly")).type()
                 != QDBusMessage::ErrorMessage,
             qPrintable(diagnostics(fixture)));
    if (!waitForItemCount(service, 1, 5000))
        qWarning() << "path-only health" << service.healthJson();
    QVERIFY2(service.itemCount() == 1, qPrintable(diagnostics(fixture)));
    key = service.typedItemModel()->keys().constFirst();
    snapshot = service.typedItemModel()->item(key);
    QVERIFY(snapshot.address.service.startsWith(QLatin1Char(':')));
    QCOMPARE(snapshot.address.service, snapshot.address.uniqueOwner);
    QCOMPARE(snapshot.address.objectPath, QStringLiteral("/org/test/Tray"));

    QVERIFY2(control.call(QStringLiteral("Exit")).type() != QDBusMessage::ErrorMessage,
             qPrintable(diagnostics(fixture)));
    QVERIFY2(fixture.waitForFinished(3000), qPrintable(diagnostics(fixture)));
    QTRY_COMPARE_WITH_TIMEOUT(service.itemCount(), 0, 4000);
    service.stop();
    QTRY_VERIFY_WITH_TIMEOUT(!QDBusConnection::sessionBus().interface()->isServiceRegistered(
                                 hostServiceName), 2000);
}

void StatusNotifierIntegrationTest::watcherAliasesRemainObservedAcrossAuthoritySwitches()
{
    QVERIFY2(!qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS").isEmpty(),
             "the mandatory integration test requires an isolated session bus");

    QProcess fixture;
    fixture.setProcessChannelMode(QProcess::SeparateChannels);
    fixture.start(QStringLiteral(ASTREA_STATUSNOTIFIER_FIXTURE_PATH));
    QVERIFY2(fixture.waitForStarted(2000), qPrintable(fixture.errorString()));

    QDBusConnectionInterface *busInterface = QDBusConnection::sessionBus().interface();
    QVERIFY(busInterface);
    QTRY_VERIFY_WITH_TIMEOUT(busInterface->isServiceRegistered(
                                 QStringLiteral("org.astrea.tests.StatusNotifierFixture")),
                             3000);
    QDBusInterface control(QStringLiteral("org.astrea.tests.StatusNotifierFixture"),
                           QStringLiteral("/org/astrea/tests/StatusNotifierFixture"),
                           QStringLiteral("org.astrea.tests.StatusNotifierFixture"),
                           QDBusConnection::sessionBus());
    QVERIFY2(control.isValid(), qPrintable(diagnostics(fixture)));

    QVERIFY2(control.call(QStringLiteral("ClaimWatcherAlias"),
                          QStringLiteral("org.kde.StatusNotifierWatcher"))
                 .arguments().value(0).toBool(),
              qPrintable(diagnostics(fixture)));
    StatusNotifierService service;
    service.start();
    QTRY_COMPARE_WITH_TIMEOUT(service.watcherMode(), WatcherMode::External, 3000);
    QVERIFY(!service.watcherOwner().isEmpty());

    QVERIFY(control.call(QStringLiteral("ClaimWatcherAlias"),
                         QStringLiteral("org.freedesktop.StatusNotifierWatcher"))
                .arguments().value(0).toBool());
    QTRY_COMPARE_WITH_TIMEOUT(service.watcherName(),
                              QStringLiteral("org.freedesktop.StatusNotifierWatcher"), 3000);

    QVERIFY(control.call(QStringLiteral("ReleaseWatcherAlias"),
                         QStringLiteral("org.kde.StatusNotifierWatcher"))
                .arguments().value(0).toBool());
    QTRY_COMPARE_WITH_TIMEOUT(service.watcherName(),
                              QStringLiteral("org.freedesktop.StatusNotifierWatcher"), 3000);

    QVERIFY(control.call(QStringLiteral("ReleaseWatcherAlias"),
                         QStringLiteral("org.freedesktop.StatusNotifierWatcher"))
                .arguments().value(0).toBool());
    QTRY_COMPARE_WITH_TIMEOUT(service.watcherMode(), WatcherMode::Owned, 3000);

    QVERIFY(control.call(QStringLiteral("Exit")).type() != QDBusMessage::ErrorMessage);
    QVERIFY2(fixture.waitForFinished(3000), qPrintable(diagnostics(fixture)));
    service.stop();
}

QTEST_GUILESS_MAIN(StatusNotifierIntegrationTest)

#include "StatusNotifierIntegrationTest.moc"
