#include "core/DockAppModel.hpp"
#include "platform/unity/UnityLauncherEntryService.hpp"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QTest>

class UnityLauncherEntryTest final : public QObject {
    Q_OBJECT

private slots:
    void updateSignalProjectsPartialStateAndSenderCleanup();
};

namespace {
constexpr auto kObjectPath = "/com/canonical/Unity/LauncherEntry";
constexpr auto kInterfaceName = "com.canonical.Unity.LauncherEntry";

void sendUpdate(const QDBusConnection &connection, const QString &desktopFileName,
                const QVariantMap &properties)
{
    QDBusMessage signal = QDBusMessage::createSignal(
        QString::fromLatin1(kObjectPath), QString::fromLatin1(kInterfaceName),
        QStringLiteral("Update"));
    signal << QStringLiteral("application://") + desktopFileName << properties;
    QVERIFY(connection.send(signal));
}

int countFor(const DockAppModel &model, const QString &desktopFileName)
{
    const int row = model.rowForDesktopFileName(desktopFileName);
    return model.data(model.index(row, 0), DockAppModel::UnreadCountRole).toInt();
}

bool visibleFor(const DockAppModel &model, const QString &desktopFileName)
{
    const int row = model.rowForDesktopFileName(desktopFileName);
    return model.data(model.index(row, 0), DockAppModel::UnreadCountVisibleRole).toBool();
}
}

void UnityLauncherEntryTest::updateSignalProjectsPartialStateAndSenderCleanup()
{
    DockAppModel model;
    model.setPins({QStringLiteral("org.example.Mail.desktop"),
                   QStringLiteral("org.example.Chat.desktop")});

    QDBusConnection session = QDBusConnection::sessionBus();
    QVERIFY(session.isConnected());
    const QString busAddress = qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS");
    QVERIFY(!busAddress.isEmpty());
    QDBusConnection competingOwner = QDBusConnection::connectToBus(
        busAddress, QStringLiteral("unity-launcher-test-existing-owner"));
    QVERIFY(competingOwner.registerService(QStringLiteral("com.canonical.Unity")));

    UnityLauncherEntryService service(&model, session);
    QVERIFY(service.start());

    QDBusConnection firstSender = QDBusConnection::connectToBus(
        busAddress, QStringLiteral("unity-launcher-test-first-sender"));
    QDBusConnection secondSender = QDBusConnection::connectToBus(
        busAddress, QStringLiteral("unity-launcher-test-second-sender"));
    QVERIFY(firstSender.isConnected());
    QVERIFY(secondSender.isConnected());

    sendUpdate(firstSender, QStringLiteral("org.example.Mail.desktop"),
               {{QStringLiteral("count"), 5},
                {QStringLiteral("count-visible"), true}});
    QTRY_COMPARE(countFor(model, QStringLiteral("org.example.Mail.desktop")), 5);
    QVERIFY(visibleFor(model, QStringLiteral("org.example.Mail.desktop")));

    sendUpdate(firstSender, QStringLiteral("org.example.Mail.desktop"),
               {{QStringLiteral("count"), 8}});
    QTRY_COMPARE(countFor(model, QStringLiteral("org.example.Mail.desktop")), 8);
    QVERIFY(visibleFor(model, QStringLiteral("org.example.Mail.desktop")));

    sendUpdate(firstSender, QStringLiteral("org.example.Mail.desktop"),
               {{QStringLiteral("count-visible"), false}});
    QTRY_VERIFY(!visibleFor(model, QStringLiteral("org.example.Mail.desktop")));
    QCOMPARE(countFor(model, QStringLiteral("org.example.Mail.desktop")), 8);

    sendUpdate(firstSender, QStringLiteral("org.example.Mail.desktop"),
               {{QStringLiteral("count"), QStringLiteral("not a number")},
                {QStringLiteral("count-visible"), QStringLiteral("false")}});
    QTest::qWait(20);
    QCOMPARE(countFor(model, QStringLiteral("org.example.Mail.desktop")), 8);
    QVERIFY(!visibleFor(model, QStringLiteral("org.example.Mail.desktop")));

    sendUpdate(firstSender, QStringLiteral("../org.example.Mail.desktop"),
               {{QStringLiteral("count"), 20}, {QStringLiteral("count-visible"), true}});
    sendUpdate(firstSender, QStringLiteral("org.example.Chat.desktop/extra"),
               {{QStringLiteral("count"), 30}, {QStringLiteral("count-visible"), true}});
    QTest::qWait(20);
    QCOMPARE(countFor(model, QStringLiteral("org.example.Mail.desktop")), 8);

    sendUpdate(secondSender, QStringLiteral("org.example.Chat.desktop"),
               {{QStringLiteral("count"), 13}, {QStringLiteral("count-visible"), true}});
    QTRY_COMPARE(countFor(model, QStringLiteral("org.example.Chat.desktop")), 13);
    QVERIFY(visibleFor(model, QStringLiteral("org.example.Chat.desktop")));

    sendUpdate(secondSender, QStringLiteral("org.example.Mail.desktop"),
               {{QStringLiteral("count"), 3},
                {QStringLiteral("count-visible"), true}});
    QTRY_COMPARE(countFor(model, QStringLiteral("org.example.Mail.desktop")), 3);
    sendUpdate(firstSender, QStringLiteral("org.example.Mail.desktop"),
               {{QStringLiteral("count"), 21}});
    QTRY_COMPARE(countFor(model, QStringLiteral("org.example.Mail.desktop")), 21);

    const QString firstOwner = firstSender.baseService();
    QVERIFY(firstOwner.startsWith(QLatin1Char(':')));
    firstSender = QDBusConnection(QStringLiteral("closed-unity-launcher-test-sender"));
    QDBusConnection::disconnectFromBus(QStringLiteral("unity-launcher-test-first-sender"));
    QTRY_VERIFY(!session.interface()->isServiceRegistered(firstOwner));
    QTRY_COMPARE(countFor(model, QStringLiteral("org.example.Mail.desktop")), 3);
    QVERIFY(visibleFor(model, QStringLiteral("org.example.Mail.desktop")));
    QCOMPARE(countFor(model, QStringLiteral("org.example.Chat.desktop")), 13);
    QVERIFY(visibleFor(model, QStringLiteral("org.example.Chat.desktop")));

    service.stop();
    QCOMPARE(countFor(model, QStringLiteral("org.example.Mail.desktop")), 0);
    QVERIFY(!visibleFor(model, QStringLiteral("org.example.Mail.desktop")));
    QCOMPARE(countFor(model, QStringLiteral("org.example.Chat.desktop")), 0);
    QVERIFY(!visibleFor(model, QStringLiteral("org.example.Chat.desktop")));
    QVERIFY(service.start());
    sendUpdate(secondSender, QStringLiteral("org.example.Mail.desktop"),
               {{QStringLiteral("count"), 7},
                {QStringLiteral("count-visible"), true}});
    QTRY_COMPARE(countFor(model, QStringLiteral("org.example.Mail.desktop")), 7);
    service.stop();
    QDBusConnection::disconnectFromBus(QStringLiteral("unity-launcher-test-second-sender"));
    competingOwner.unregisterService(QStringLiteral("com.canonical.Unity"));
    QDBusConnection::disconnectFromBus(QStringLiteral("unity-launcher-test-existing-owner"));
}

QTEST_GUILESS_MAIN(UnityLauncherEntryTest)

#include "UnityLauncherEntryTest.moc"
