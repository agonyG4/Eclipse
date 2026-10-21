#include "core/BarStatusOrderStore.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TopbarReorderTest final : public QObject {
    Q_OBJECT

private slots:
    void newItemsUseDefaultSlotsWithoutWritingConfiguration();
    void trayOrderKeysSurviveAddressOwnerChanges();
    void committedVisibleOrderPersistsAndKeepsAbsentItems();
    void failedSaveReportsErrorWithoutChangingOrder();
};

void TopbarReorderTest::newItemsUseDefaultSlotsWithoutWritingConfiguration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = QDir(directory.path()).filePath(QStringLiteral("order.json"));
    BarStatusOrderStore store(path);

    QCOMPARE(store.order(), QStringList({QStringLiteral("builtin:network"),
                                         QStringLiteral("builtin:bluetooth"),
                                         QStringLiteral("builtin:volume"),
                                         QStringLiteral("builtin:clock")}));
    store.registerItem(QStringLiteral("tray:chat:/StatusNotifierItem"));
    store.registerItem(QStringLiteral("tray:music:/StatusNotifierItem"));

    QCOMPARE(store.order(), QStringList({QStringLiteral("tray:chat:/StatusNotifierItem"),
                                         QStringLiteral("tray:music:/StatusNotifierItem"),
                                         QStringLiteral("builtin:network"),
                                         QStringLiteral("builtin:bluetooth"),
                                         QStringLiteral("builtin:volume"),
                                         QStringLiteral("builtin:clock")}));
    QVERIFY(!QFileInfo::exists(path));
}

void TopbarReorderTest::trayOrderKeysSurviveAddressOwnerChanges()
{
    BarStatusOrderStore store;

    QCOMPARE(store.trayKey(QStringLiteral("org.example.Chat"),
                           QStringLiteral("org.example.Chat|/StatusNotifierItem")),
             QStringLiteral("tray:org.example.Chat:/StatusNotifierItem"));
    QCOMPARE(store.trayKey(QStringLiteral("org.example.Chat"),
                           QStringLiteral(":1.42|/StatusNotifierItem")),
             QStringLiteral("tray:org.example.Chat:/StatusNotifierItem"));
    QCOMPARE(store.trayKey({}, QStringLiteral(":1.42|/StatusNotifierItem")),
             QStringLiteral("tray-fallback::1.42|/StatusNotifierItem"));
}

void TopbarReorderTest::committedVisibleOrderPersistsAndKeepsAbsentItems()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = QDir(directory.path()).filePath(QStringLiteral("order.json"));

    {
        BarStatusOrderStore store(path);
        store.registerItem(QStringLiteral("tray:chat:/StatusNotifierItem"));
        store.registerItem(QStringLiteral("tray:music:/StatusNotifierItem"));
        QVERIFY(store.commitVisibleOrder({QStringLiteral("builtin:network"),
                                          QStringLiteral("tray:chat:/StatusNotifierItem"),
                                          QStringLiteral("tray:music:/StatusNotifierItem"),
                                          QStringLiteral("builtin:bluetooth"),
                                          QStringLiteral("builtin:volume"),
                                          QStringLiteral("builtin:clock")}));
        QVERIFY(QFileInfo::exists(path));
    }

    BarStatusOrderStore restored(path);
    QCOMPARE(restored.order(), QStringList({QStringLiteral("builtin:network"),
                                            QStringLiteral("tray:chat:/StatusNotifierItem"),
                                            QStringLiteral("tray:music:/StatusNotifierItem"),
                                            QStringLiteral("builtin:bluetooth"),
                                            QStringLiteral("builtin:volume"),
                                            QStringLiteral("builtin:clock")}));

    QVERIFY(restored.commitVisibleOrder({QStringLiteral("tray:music:/StatusNotifierItem"),
                                         QStringLiteral("builtin:network")}));
    QCOMPARE(restored.order(), QStringList({QStringLiteral("tray:music:/StatusNotifierItem"),
                                            QStringLiteral("tray:chat:/StatusNotifierItem"),
                                            QStringLiteral("builtin:network"),
                                            QStringLiteral("builtin:bluetooth"),
                                            QStringLiteral("builtin:volume"),
                                            QStringLiteral("builtin:clock")}));
}

void TopbarReorderTest::failedSaveReportsErrorWithoutChangingOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = QDir(directory.path()).filePath(QStringLiteral("order.json"));
    QVERIFY(QDir().mkpath(path));

    BarStatusOrderStore store(path);
    const QStringList original = store.order();
    QSignalSpy failures(&store, &BarStatusOrderStore::persistenceFailed);
    QSignalSpy changes(&store, &BarStatusOrderStore::orderChanged);

    QVERIFY(!store.commitVisibleOrder({QStringLiteral("tray:new:/StatusNotifierItem"),
                                       QStringLiteral("tray:new:/StatusNotifierItem")}));
    QCOMPARE(store.order(), original);
    QCOMPARE(changes.count(), 0);

    QVERIFY(!store.commitVisibleOrder({QStringLiteral("tray:new:/StatusNotifierItem"),
                                       QStringLiteral("builtin:clock"),
                                       QStringLiteral("builtin:network"),
                                       QStringLiteral("builtin:bluetooth"),
                                       QStringLiteral("builtin:volume")}));
    QCOMPARE(store.order(), original);
    QCOMPARE(changes.count(), 0);
    QVERIFY(!store.lastError().isEmpty());
    QCOMPARE(failures.count(), 2);
}

QTEST_GUILESS_MAIN(TopbarReorderTest)

#include "TopbarReorderTest.moc"
