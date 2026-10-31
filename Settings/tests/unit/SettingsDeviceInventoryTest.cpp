#include "services/devices/SettingsDeviceInventory.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class SettingsDeviceInventoryTest : public QObject {
    Q_OBJECT
private slots:
    void enumeratesInputUsbAndStorageWithoutHubsOrPartitions();
    void returnsEmptyForUnavailableRoot();
};

namespace {
void writeAttr(const QString &root, const QString &relative, const QByteArray &data)
{
    const QString path = root + QLatin1Char('/') + relative;
    QVERIFY(QDir().mkpath(QFileInfo(path).path()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(data) == data.size());
}
}

void SettingsDeviceInventoryTest::enumeratesInputUsbAndStorageWithoutHubsOrPartitions()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = dir.path();

    writeAttr(root, QStringLiteral("bus/usb/devices/1-2/idVendor"), "abcd\n");
    writeAttr(root, QStringLiteral("bus/usb/devices/1-2/idProduct"), "1234\n");
    writeAttr(root, QStringLiteral("bus/usb/devices/1-2/product"), "Mechanical Keyboard\n");
    writeAttr(root, QStringLiteral("bus/usb/devices/1-3/idVendor"), "abcd\n");
    writeAttr(root, QStringLiteral("bus/usb/devices/1-3/idProduct"), "5678\n");
    writeAttr(root, QStringLiteral("bus/usb/devices/1-3/product"), "USB camera\n");
    writeAttr(root, QStringLiteral("bus/usb/devices/usb1/idVendor"), "abcd\n");
    writeAttr(root, QStringLiteral("bus/usb/devices/usb1/idProduct"), "9999\n");
    writeAttr(root, QStringLiteral("bus/usb/devices/usb1/bDeviceClass"), "09\n");
    writeAttr(root, QStringLiteral("class/block/sdb/size"), "2097152\n");
    writeAttr(root, QStringLiteral("class/block/sdb/removable"), "1\n");
    writeAttr(root, QStringLiteral("class/block/sdb/device/model"), "Flash Drive\n");
    writeAttr(root, QStringLiteral("class/block/sdb1/size"), "1048576\n");
    writeAttr(root, QStringLiteral("class/block/sdb1/partition"), "1\n");
    writeAttr(root, QStringLiteral("class/input/event4/device/name"), "AT Translated Set 2 keyboard\n");

    const QVariantList list = SettingsDeviceInventory::enumerate(root);
    QCOMPARE(list.size(), 4);
    QSet<QString> ids;
    for (const QVariant &item : list)
        ids.insert(item.toMap().value(QStringLiteral("id")).toString());
    QVERIFY(ids.contains(QStringLiteral("usb:1-2:abcd:1234")));
    QVERIFY(ids.contains(QStringLiteral("usb:1-3:abcd:5678")));
    QVERIFY(ids.contains(QStringLiteral("block:sdb")));
    QVERIFY(ids.contains(QStringLiteral("input:") + root
                         + QStringLiteral("/class/input/event4/device")));
    for (const QVariant &item : list) {
        const auto device = item.toMap();
        QVERIFY(!device.value(QStringLiteral("name")).toString().isEmpty());
        QVERIFY(!device.value(QStringLiteral("iconKey")).toString().isEmpty());
    }
}

void SettingsDeviceInventoryTest::returnsEmptyForUnavailableRoot()
{
    QCOMPARE(SettingsDeviceInventory::enumerate(QStringLiteral("/does-not-exist-astrea-devices-test")),
             QVariantList());
}

QTEST_GUILESS_MAIN(SettingsDeviceInventoryTest)
#include "SettingsDeviceInventoryTest.moc"
