#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>

#include "platform/wayland/DockInputRegionBridge.hpp"
#include "platform/wayland/DockInputRegionPolicy.hpp"

class DockInputRegionTest final : public QObject {
    Q_OBJECT

private slots:
    void restingChromeIsContained();
    void magnifiedIconAddsOnlyItsInteractiveHeadroom();
    void multipleApplicationRegionsAreUnited();
    void regionsAreClippedToSurfaceBounds();
    void invalidAndExcessRegionsAreIgnored();
    void bridgeAppliesAndCachesWindowMask();
    void qmlRectanglesReachBridge();
};

void DockInputRegionTest::restingChromeIsContained()
{
    const QRegion region = DockInputRegionPolicy::regionFor(
        QSize(300, 70), QRectF(0, 0, 300, 70), {});

    QVERIFY(region.contains(QPoint(0, 0)));
    QVERIFY(region.contains(QPoint(299, 69)));
    QVERIFY(!region.contains(QPoint(300, 0)));
    QVERIFY(!region.contains(QPoint(0, 70)));
}

void DockInputRegionTest::magnifiedIconAddsOnlyItsInteractiveHeadroom()
{
    const QRegion region = DockInputRegionPolicy::regionFor(
        QSize(300, 120), QRectF(0, 50, 300, 70), {QRectF(126, 8, 48, 48)});

    QVERIFY(region.contains(QPoint(150, 20)));
    QVERIFY(region.contains(QPoint(20, 80)));
    QVERIFY(!region.contains(QPoint(40, 20)));
}

void DockInputRegionTest::multipleApplicationRegionsAreUnited()
{
    const QRegion region = DockInputRegionPolicy::regionFor(
        QSize(300, 120), QRectF(100, 90, 100, 30),
        {QRectF(20, 40, 40, 40), QRectF(240, 45, 40, 40)});

    QVERIFY(region.contains(QPoint(30, 50)));
    QVERIFY(region.contains(QPoint(250, 55)));
    QVERIFY(region.contains(QPoint(150, 100)));
    QVERIFY(!region.contains(QPoint(150, 45)));
}

void DockInputRegionTest::regionsAreClippedToSurfaceBounds()
{
    const QRegion region = DockInputRegionPolicy::regionFor(
        QSize(300, 120), QRectF(-20, 100, 50, 50), {});

    QVERIFY(region.contains(QPoint(0, 110)));
    QVERIFY(region.contains(QPoint(29, 119)));
    QVERIFY(!region.contains(QPoint(30, 119)));
    QVERIFY(!region.contains(QPoint(0, 120)));
}

void DockInputRegionTest::invalidAndExcessRegionsAreIgnored()
{
    const qreal nan = qQNaN();
    const qreal infinity = qInf();
    const QRegion region = DockInputRegionPolicy::regionFor(
        QSize(300, 120), QRectF(),
        {QRectF(50, 50, 20, 20), QRectF(nan, 10, 20, 20),
         QRectF(infinity, 10, 20, 20), QRectF(250, 110, 100, 100)},
        3);

    QVERIFY(region.contains(QPoint(55, 55)));
    QVERIFY(!region.contains(QPoint(270, 115)));
    QVERIFY(!region.contains(QPoint(0, 0)));
}

void DockInputRegionTest::bridgeAppliesAndCachesWindowMask()
{
    QQuickWindow window;
    window.resize(300, 120);
    DockInputRegionBridge bridge;
    bridge.setWindow(&window);
    QSignalSpy appliedSpy(&bridge, &DockInputRegionBridge::regionApplied);
    QVERIFY(appliedSpy.isValid());

    const QRectF chrome(0, 50, 300, 70);
    QVariantList interactionRects;
    interactionRects.append(QVariant::fromValue(QRectF(126, 8, 48, 48)));
    bridge.update(chrome, interactionRects, 1);
    const QRegion expected = DockInputRegionPolicy::regionFor(
        window.size(), chrome, {QRectF(126, 8, 48, 48)});
    QCOMPARE(window.mask(), expected);
    QCOMPARE(appliedSpy.count(), 1);

    bridge.update(chrome, interactionRects, 1);
    QCOMPARE(window.mask(), expected);
    QCOMPARE(appliedSpy.count(), 1);

    bridge.update(chrome, interactionRects, 0);
    QCOMPARE(window.mask(), DockInputRegionPolicy::regionFor(window.size(), chrome, {}));
    QCOMPARE(appliedSpy.count(), 2);
}

void DockInputRegionTest::qmlRectanglesReachBridge()
{
    QQuickWindow window;
    window.resize(300, 120);
    DockInputRegionBridge bridge;
    bridge.setWindow(&window);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("inputBridge"), &bridge);
    QQmlComponent component(&engine);
    component.setData(R"qml(
        import QtQml
        QtObject {
            Component.onCompleted: inputBridge.update(
                Qt.rect(0, 50, 300, 70), [Qt.rect(126, 8, 48, 48)], 1)
        }
    )qml", QUrl(QStringLiteral("qrc:/dock-input-region-test.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    QObject *object = component.create();
    QVERIFY2(object, qPrintable(component.errorString()));
    QCoreApplication::processEvents();

    const QRegion expected = DockInputRegionPolicy::regionFor(
        window.size(), QRectF(0, 50, 300, 70), {QRectF(126, 8, 48, 48)});
    QCOMPARE(window.mask(), expected);
    delete object;
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication application(argc, argv);
    DockInputRegionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "DockInputRegionTest.moc"
