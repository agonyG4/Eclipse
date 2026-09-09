#include "platform/wayland/effects/RoundedEffectRegion.hpp"

#include <QTest>

#include <algorithm>

namespace {

bool covered(const QVector<QRect> &rectangles, const int x, const int y)
{
    return std::any_of(rectangles.cbegin(), rectangles.cend(), [point = QPoint(x, y)](
                                                                  const QRect &rectangle) {
        return rectangle.contains(point);
    });
}

void verifyRectangles(const QSize &size, const QVector<QRect> &rectangles)
{
    QVERIFY(!rectangles.isEmpty());
    for (const QRect &rectangle : rectangles) {
        QVERIFY(rectangle.width() > 0);
        QVERIFY(rectangle.height() > 0);
        QVERIFY(rectangle.left() >= 0);
        QVERIFY(rectangle.top() >= 0);
        QVERIFY(rectangle.right() < size.width());
        QVERIFY(rectangle.bottom() < size.height());
    }
}

} // namespace

class RoundedEffectRegionTest final : public QObject {
    Q_OBJECT

private slots:
    void roundedRegionsStayWithinWaylandRectangleBudget()
    {
        const auto rectangles = AstreaRoundedEffectRegion::rectangles(QSize(640, 360), 24.0);
        QVERIFY(!rectangles.isEmpty());
        QVERIFY(rectangles.size() <= 33);
        QVERIFY(rectangles.size() < 128);
    }

    void roundedRegionsCoverCommonRadiiWithoutGaps()
    {
        const QSize size(640, 360);
        for (const int radius : {1, 8, 18, 24, 25, 31, 32}) {
            const auto rectangles = AstreaRoundedEffectRegion::rectangles(size, radius);
            verifyRectangles(size, rectangles);
            QVERIFY(rectangles.size() <= 33);
            QVERIFY(rectangles.size() < 128);

            for (int y = 0; y < size.height(); ++y) {
                bool rowCovered = false;
                for (int x = 0; x < size.width(); ++x) {
                    const bool pixelCovered = covered(rectangles, x, y);
                    rowCovered = rowCovered || pixelCovered;
                    QCOMPARE(pixelCovered, covered(rectangles, size.width() - 1 - x, y));
                    QCOMPARE(pixelCovered,
                             covered(rectangles, x, size.height() - 1 - y));
                }
                QVERIFY(rowCovered);
            }

            const int centerY = size.height() / 2;
            for (int x = 0; x < size.width(); ++x)
                QVERIFY(covered(rectangles, x, centerY));
            QVERIFY(!covered(rectangles, 0, 0));
            QVERIFY(!covered(rectangles, size.width() - 1, 0));
            QVERIFY(!covered(rectangles, 0, size.height() - 1));
            QVERIFY(!covered(rectangles, size.width() - 1, size.height() - 1));
        }
    }

    void tinySurfacesClampRadiusAndRemainCovered()
    {
        for (const QSize size : {QSize(2, 2), QSize(3, 3), QSize(5, 3), QSize(3, 5)}) {
            const auto rectangles = AstreaRoundedEffectRegion::rectangles(size, 32.0);
            verifyRectangles(size, rectangles);
            for (int y = 0; y < size.height(); ++y) {
                bool rowCovered = false;
                for (int x = 0; x < size.width(); ++x)
                    rowCovered = rowCovered || covered(rectangles, x, y);
                QVERIFY(rowCovered);
            }
        }
    }

    void zeroRadiusUsesTheWholeSurface()
    {
        QCOMPARE(AstreaRoundedEffectRegion::rectangles(QSize(100, 60), 0.0),
                 QVector<QRect>{QRect(0, 0, 100, 60)});
    }
};

QTEST_MAIN(RoundedEffectRegionTest)
#include "RoundedEffectRegionTest.moc"
