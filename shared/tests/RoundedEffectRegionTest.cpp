#include "platform/wayland/effects/RoundedEffectRegion.hpp"

#include <QTest>

class RoundedEffectRegionTest final : public QObject {
    Q_OBJECT

private slots:
    void roundedRegionsStayWithinWaylandRectangleBudget()
    {
        const auto rectangles = AstreaRoundedEffectRegion::rectangles(QSize(640, 360), 24.0);
        QVERIFY(!rectangles.isEmpty());
        QVERIFY(rectangles.size() <= 33);
    }

    void zeroRadiusUsesTheWholeSurface()
    {
        QCOMPARE(AstreaRoundedEffectRegion::rectangles(QSize(100, 60), 0.0),
                 QVector<QRect>{QRect(0, 0, 100, 60)});
    }
};

QTEST_MAIN(RoundedEffectRegionTest)
#include "RoundedEffectRegionTest.moc"
