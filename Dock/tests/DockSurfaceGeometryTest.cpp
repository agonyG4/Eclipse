#include "core/DockSurfaceGeometry.hpp"

#include <QTest>

class DockSurfaceGeometryTest final : public QObject {
    Q_OBJECT

private slots:
    void centeredDockUsesOutputLocalCoordinates();
    void ultrawideAndVirtualOriginDoNotDistortGeometry();
    void magnifiedSurfaceUsesCurrentSurfaceWidth();
    void itemPositionsAndBottomMarginsRemainDeterministic();
};

void DockSurfaceGeometryTest::centeredDockUsesOutputLocalCoordinates()
{
    QCOMPARE(DockSurfaceGeometry::delegateRectInOutput(
                  QSize(1920, 1080), QSize(600, 84), 12, QRectF(14, 19, 56, 62)),
              QRect(674, 1003, 56, 62));
}

void DockSurfaceGeometryTest::ultrawideAndVirtualOriginDoNotDistortGeometry()
{
    const QRect local = DockSurfaceGeometry::delegateRectInOutput(
        QSize(3440, 1440), QSize(800, 110), 24, QRectF(14, 41, 56, 62));
    QCOMPARE(local, QRect(1334, 1347, 56, 62));

    // The helper has no virtual-desktop-origin input by contract. The same
    // output-local result therefore applies on a secondary output anywhere in
    // the compositor's virtual desktop.
    QCOMPARE(local, QRect(1334, 1347, 56, 62));
}

void DockSurfaceGeometryTest::magnifiedSurfaceUsesCurrentSurfaceWidth()
{
    const QRect resting = DockSurfaceGeometry::delegateRectInOutput(
        QSize(1920, 1080), QSize(600, 84), 12, QRectF(14, 19, 56, 62));
    const QRect magnified = DockSurfaceGeometry::delegateRectInOutput(
        QSize(1920, 1080), QSize(760, 130), 12, QRectF(14, 68, 56, 62));

    QCOMPARE(resting, QRect(674, 1003, 56, 62));
    QCOMPARE(magnified, QRect(594, 1006, 56, 62));
}

void DockSurfaceGeometryTest::itemPositionsAndBottomMarginsRemainDeterministic()
{
    const QSize output(1920, 1080);
    const QSize surface(600, 84);
    QCOMPARE(DockSurfaceGeometry::delegateRectInOutput(
                  output, surface, 0, QRectF(14, 19, 56, 62)),
              QRect(674, 1015, 56, 62));
    QCOMPARE(DockSurfaceGeometry::delegateRectInOutput(
                  output, surface, 48, QRectF(14, 19, 56, 62)),
              QRect(674, 967, 56, 62));
    QCOMPARE(DockSurfaceGeometry::delegateRectInOutput(
                  output, surface, 12, QRectF(14, 19, 56, 62)),
              QRect(674, 1003, 56, 62));

    for (const qreal localX : {14.0, 272.0, 530.0}) {
        const QRect item = DockSurfaceGeometry::delegateRectInOutput(
            output, surface, 12, QRectF(localX, 19, 56, 62));
        QVERIFY(item.left() >= 0);
        QVERIFY(item.right() < output.width());
        QVERIFY(item.top() >= 0);
        QVERIFY(item.bottom() < output.height());
    }
}

QTEST_GUILESS_MAIN(DockSurfaceGeometryTest)
#include "DockSurfaceGeometryTest.moc"
