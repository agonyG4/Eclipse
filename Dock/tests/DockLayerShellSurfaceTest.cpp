#include <QTest>

#include "core/DockMetrics.hpp"
#include "platform/wayland/DockLayerShellSurface.hpp"

class DockLayerShellSurfaceTest final : public QObject {
    Q_OBJECT

private slots:
    void mappedReservationUsesRestingHeightNotVisualHeight();
    void unmappedReservationIsZero();
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

QTEST_GUILESS_MAIN(DockLayerShellSurfaceTest)
#include "DockLayerShellSurfaceTest.moc"
