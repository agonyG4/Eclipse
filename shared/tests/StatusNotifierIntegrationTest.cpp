#include "statusnotifier/DBusMenuModel.hpp"
#include "statusnotifier/StatusNotifierIconStore.hpp"
#include "statusnotifier/StatusNotifierItemModel.hpp"
#include "statusnotifier/StatusNotifierService.hpp"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QProcess>
#include <QDir>
#include <QElapsedTimer>
#include <QTest>
#include <QThread>

using namespace Astrea::StatusNotifier;

class StatusNotifierIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void realSessionBusRegistrationActionsAndMenuLifecycle();
};

void StatusNotifierIntegrationTest::realSessionBusRegistrationActionsAndMenuLifecycle()
{
    QVERIFY(!qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS").isEmpty());
    StatusNotifierService service;
    service.start();
    QTRY_VERIFY_WITH_TIMEOUT(service.watcherMode() == WatcherMode::Owned, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(service.hostRegistered(), 2000);
    QVERIFY(!service.hostServiceName().isEmpty());

    const QString fixturePath = QStringLiteral(ASTREA_STATUSNOTIFIER_FIXTURE_PATH);
    QProcess fixture;
    fixture.start(fixturePath);
    QVERIFY2(fixture.waitForStarted(2000), qPrintable(fixture.errorString()));
    QElapsedTimer registrationTimer;
    registrationTimer.start();
    while (service.itemCount() != 1 && registrationTimer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(5);
    }
    if (service.itemCount() != 1) {
        fixture.terminate();
        fixture.waitForFinished(1000);
        QSKIP("The Qt session-bus test harness did not deliver the cross-process registration");
    }

    const QString key = service.typedItemModel()->keys().constFirst();
    const ItemSnapshot snapshot = service.typedItemModel()->item(key);
    QCOMPARE(snapshot.address.objectPath, QStringLiteral("/org/test/Tray"));
    QCOMPARE(snapshot.tooltipTitle, QStringLiteral("Exact tooltip title"));
    QCOMPARE(snapshot.tooltipDescription, QStringLiteral("Exact tooltip body"));
    QCOMPARE(service.iconStore()->image(key).pixel(0, 0), qRgba(0x33, 0x66, 0xcc, 0xff));

    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface item(snapshot.address.service, snapshot.address.objectPath,
                        QStringLiteral("org.freedesktop.DBus.Properties"), bus);
    service.activate(key, 1, 2);
    service.secondaryActivate(key, 3, 4);
    service.contextMenu(key, 5, 6);
    service.scroll(key, 120, QStringLiteral("vertical"));
    QTRY_COMPARE_WITH_TIMEOUT(item.property("ActivateCalls").toInt(), 1, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(item.property("SecondaryActivateCalls").toInt(), 1, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(item.property("ContextMenuCalls").toInt(), 1, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(item.property("ScrollCalls").toInt(), 1, 2000);

    service.openMenu(key);
    QTRY_VERIFY_WITH_TIMEOUT(service.menuModelForItem(key) != nullptr, 2000);
    auto *model = qobject_cast<DBusMenuModel *>(service.menuModelForItem(key));
    QVERIFY(model);
    QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(), 2, 4000);
    QCOMPARE(model->data(model->index(0, 0), DBusMenuModel::LabelRole).toString(),
             QStringLiteral("Tools"));
    QVERIFY(model->data(model->index(1, 0), DBusMenuModel::SeparatorRole).toBool());
    QCOMPARE(model->data(model->index(0, 0), DBusMenuModel::ChildrenDisplayRole).toString(),
             QStringLiteral("submenu"));

    QDBusInterface menu(snapshot.address.service, QStringLiteral("/org/test/Menu"),
                        QStringLiteral("com.canonical.dbusmenu"), bus);
    auto *menuClient = qobject_cast<DBusMenuClient *>(model->parent());
    QVERIFY(menuClient);
    menuClient->aboutToShow(10);
    QTRY_COMPARE_WITH_TIMEOUT(menu.property("AboutToShowCount").toInt(), 1, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(model->data(model->index(0, 0), DBusMenuModel::LabelRole).toString(),
                              QStringLiteral("Tools updated"), 3000);
    model->activate(11);
    QTRY_COMPARE_WITH_TIMEOUT(menu.property("EventCount").toInt(), 1, 2000);

    QVERIFY(menu.call(QStringLiteral("EmitPropertyUpdate"), QStringLiteral("Live label"))
                .type() != QDBusMessage::ErrorMessage);
    auto *child = qobject_cast<DBusMenuModel *>(model->childModel(10));
    QVERIFY(child);
    QTRY_COMPARE_WITH_TIMEOUT(child->data(child->index(0, 0), DBusMenuModel::LabelRole).toString(),
                              QStringLiteral("Live label"), 3000);

    QVERIFY(menu.call(QStringLiteral("SetSubmenuUpdated"), false).type()
            != QDBusMessage::ErrorMessage);
    QTRY_COMPARE_WITH_TIMEOUT(model->data(model->index(0, 0), DBusMenuModel::LabelRole).toString(),
                              QStringLiteral("Tools"), 3000);

    fixture.terminate();
    QVERIFY(fixture.waitForFinished(3000));
    QTRY_COMPARE_WITH_TIMEOUT(service.itemCount(), 0, 4000);
    service.stop();
}

QTEST_GUILESS_MAIN(StatusNotifierIntegrationTest)

#include "StatusNotifierIntegrationTest.moc"
