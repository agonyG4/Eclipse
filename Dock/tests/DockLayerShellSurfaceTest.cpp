#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTest>

#include "core/DockMetrics.hpp"
#include "platform/wayland/DockLayerShellSurface.hpp"

class DockLayerShellSurfaceTest final : public QObject {
    Q_OBJECT

private slots:
    void mappedReservationUsesRestingHeightNotVisualHeight();
    void unmappedReservationIsZero();
    void explicitOutputGeometryDoesNotDependOnWindowScreen();
};

void DockLayerShellSurfaceTest::mappedReservationUsesRestingHeightNotVisualHeight()
{
    const int iconSize = 48;
    const int restingHeight = DockMetrics::restingHeight(iconSize);
    const int magnifiedVisualHeight = restingHeight + 28;

    QVERIFY(magnifiedVisualHeight > restingHeight);
    // The mapping API receives the explicit resting contract. The visual height
    // is deliberately not an input to the reservation calculation.
    QCOMPARE(DockLayerShellSurface::exclusiveZoneForMapping(true, restingHeight), restingHeight);
    QCOMPARE(DockLayerShellSurface::exclusiveZoneForMapping(true, restingHeight),
             DockMetrics::restingHeight(iconSize));
}

void DockLayerShellSurfaceTest::unmappedReservationIsZero()
{
    QCOMPARE(DockLayerShellSurface::exclusiveZoneForMapping(false, 68), 0);
    QCOMPARE(DockLayerShellSurface::exclusiveZoneForMapping(false, 96), 0);
}

void DockLayerShellSurfaceTest::explicitOutputGeometryDoesNotDependOnWindowScreen()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"qml(
        import QtQuick
        import QtQuick.Window

        Window {
            property string outputKey: ""
            property int outputWidth: 1
            property int outputHeight: 1
            property int outputOriginX: 0
            property int outputOriginY: 0
        }
    )qml", QUrl(QStringLiteral("DockOutputGeometryTest.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(component.create());
    QVERIFY2(window, qPrintable(component.errorString()));

    QScreen *screen = QGuiApplication::primaryScreen();
    QVERIFY(screen);
    const QRect geometry = screen->geometry();
    QVERIFY(DockLayerShellSurface::setOutputGeometry(window, screen));
    QCOMPARE(window->property("outputKey").toString(), screen->name());
    QCOMPARE(window->property("outputWidth").toInt(), geometry.width());
    QCOMPARE(window->property("outputHeight").toInt(), geometry.height());
    QCOMPARE(window->property("outputOriginX").toInt(), geometry.x());
    QCOMPARE(window->property("outputOriginY").toInt(), geometry.y());

    delete window;
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication application(argc, argv);
    DockLayerShellSurfaceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "DockLayerShellSurfaceTest.moc"
