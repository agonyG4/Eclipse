#include "core/DockSurfaceGeometry.hpp"

#include <QMetaObject>
#include <QTest>
#include <QVariantMap>

namespace {

QVariantMap tooltipPlacement(DockSurfaceGeometry &geometry, int outputWidth,
                             int outputHeight, const QRectF &anchorRect, int cardWidth,
                             int cardHeight, const QString &position, bool *invoked)
{
    QVariantMap result;
    *invoked = QMetaObject::invokeMethod(
        &geometry, "tooltipPlacementInOutput", Qt::DirectConnection,
        Q_RETURN_ARG(QVariantMap, result), Q_ARG(int, outputWidth),
        Q_ARG(int, outputHeight), Q_ARG(QRectF, anchorRect), Q_ARG(int, cardWidth),
        Q_ARG(int, cardHeight), Q_ARG(QString, position));
    return result;
}

} // namespace

class DockSurfaceGeometryTest final : public QObject {
    Q_OBJECT

private slots:
    void centeredDockUsesOutputLocalCoordinates();
    void ultrawideAndVirtualOriginDoNotDistortGeometry();
    void magnifiedSurfaceUsesCurrentSurfaceWidth();
    void itemPositionsAndBottomMarginsRemainDeterministic();
    void verticalEdgesUseInwardOutputLocalOrigins();
    void physicalEdgeSurfaceDoesNotDoubleApplyFloatingMargin();
    void restingBottomAnchorUsesUnscaledIconGeometry();
    void restingVerticalAnchorsUseGlobalOutputOrigins();
    void restingAnchorIgnoresMagnificationScale();
    void tooltipPlacementSupportsEveryDockEdge();
    void tooltipPlacementClampsAndFlipsNearOutputEdges();
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

void DockSurfaceGeometryTest::verticalEdgesUseInwardOutputLocalOrigins()
{
    const QSize output(1920, 1080);
    const QSize surface(100, 600);
    const QRectF delegate(14, 19, 56, 62);

    QCOMPARE(DockSurfaceGeometry::delegateRectInOutput(
                  output, surface, QStringLiteral("left"), 12, delegate),
              QRect(26, 259, 56, 62));
    QCOMPARE(DockSurfaceGeometry::delegateRectInOutput(
                  output, surface, QStringLiteral("right"), 12, delegate),
              QRect(1822, 259, 56, 62));
    QCOMPARE(DockSurfaceGeometry::delegateRectInOutput(
                  output, surface, QStringLiteral("left"), 0, delegate),
              QRect(14, 259, 56, 62));
}

void DockSurfaceGeometryTest::physicalEdgeSurfaceDoesNotDoubleApplyFloatingMargin()
{
    const QSize output(1920, 1080);
    const QSize surface(100, 600);
    const QRectF delegate(14, 19, 56, 62);

    QCOMPARE(DockSurfaceGeometry::delegateRectInOutput(
                  output, surface, QStringLiteral("left"), 0, delegate),
              QRect(14, 259, 56, 62));
    QCOMPARE(DockSurfaceGeometry::delegateRectInOutput(
                  output, surface, QStringLiteral("right"), 0, delegate),
              QRect(1834, 259, 56, 62));
    QCOMPARE(DockSurfaceGeometry::delegateRectInOutput(
                  output, QSize(600, 84), QStringLiteral("bottom"), 0,
                  QRectF(272, 19, 56, 62)),
              QRect(932, 1015, 56, 62));
}

void DockSurfaceGeometryTest::restingBottomAnchorUsesUnscaledIconGeometry()
{
    // Three 56px delegates, 10px apart, with 14px panel padding and a 48px
    // resting icon. The anchor is the icon's stable bottom-dock rectangle.
    const QSize output(1920, 1080);
    const QSize surface(216, 68);
    const QRect first = DockSurfaceGeometry::restingIconRectInGlobal(
        output, surface, QPoint(0, 0), QStringLiteral("bottom"), 12, 0, 48, 56, 62, 10,
        14, 0, 3);
    const QRect last = DockSurfaceGeometry::restingIconRectInGlobal(
        output, surface, QPoint(0, 0), QStringLiteral("bottom"), 12, 0, 48, 56, 62, 10,
        14, 2, 3);

    QCOMPARE(first, QRect(870, 1010, 48, 48));
    QCOMPARE(last, QRect(1002, 1010, 48, 48));
}

void DockSurfaceGeometryTest::restingVerticalAnchorsUseGlobalOutputOrigins()
{
    const QSize output(1920, 1080);
    const QSize surface(68, 234);

    QCOMPARE(DockSurfaceGeometry::restingIconRectInGlobal(
                 output, surface, QPoint(320, 40), QStringLiteral("left"), 12, 0, 48, 56, 62,
                 10, 14, 0, 3),
             QRect(342, 484, 48, 48));
    QCOMPARE(DockSurfaceGeometry::restingIconRectInGlobal(
                 output, surface, QPoint(-1920, -100), QStringLiteral("right"), 12, 0, 48, 56,
                 62, 10, 14, 2, 3),
             QRect(-70, 488, 48, 48));
}

void DockSurfaceGeometryTest::restingAnchorIgnoresMagnificationScale()
{
    const auto resting = [](double magnificationScale) {
        Q_UNUSED(magnificationScale);
        return DockSurfaceGeometry::restingIconRectInGlobal(
            QSize(1920, 1080), QSize(216, 68), QPoint(0, 0), QStringLiteral("bottom"), 12, 0,
            48, 56, 62, 10, 14, 1, 3);
    };

    QCOMPARE(resting(1.0), resting(1.6));
    QCOMPARE(resting(1.6), resting(2.0));
}

void DockSurfaceGeometryTest::tooltipPlacementSupportsEveryDockEdge()
{
    DockSurfaceGeometry geometry;
    bool invoked = false;
    const QVariantMap bottom = tooltipPlacement(
        geometry, 800, 600, QRectF(376, 500, 48, 48), 180, 34,
        QStringLiteral("bottom"), &invoked);
    QVERIFY2(invoked, "DockSurfaceGeometry must expose tooltip placement to QML");
    QCOMPARE(bottom.value(QStringLiteral("cardRect")).toRect(), QRect(310, 450, 180, 34));
    QCOMPARE(bottom.value(QStringLiteral("pointerEdge")).toString(), QStringLiteral("bottom"));
    QCOMPARE(bottom.value(QStringLiteral("pointerCenter")).toInt(), 90);

    const QVariantMap left = tooltipPlacement(
        geometry, 800, 600, QRectF(10, 250, 48, 48), 180, 34,
        QStringLiteral("left"), &invoked);
    QVERIFY(invoked);
    QCOMPARE(left.value(QStringLiteral("cardRect")).toRect(), QRect(74, 257, 180, 34));
    QCOMPARE(left.value(QStringLiteral("pointerEdge")).toString(), QStringLiteral("left"));
    QCOMPARE(left.value(QStringLiteral("pointerCenter")).toInt(), 17);

    const QVariantMap right = tooltipPlacement(
        geometry, 800, 600, QRectF(742, 250, 48, 48), 180, 34,
        QStringLiteral("right"), &invoked);
    QVERIFY(invoked);
    QCOMPARE(right.value(QStringLiteral("cardRect")).toRect(), QRect(546, 257, 180, 34));
    QCOMPARE(right.value(QStringLiteral("pointerEdge")).toString(), QStringLiteral("right"));
    QCOMPARE(right.value(QStringLiteral("pointerCenter")).toInt(), 17);
}

void DockSurfaceGeometryTest::tooltipPlacementClampsAndFlipsNearOutputEdges()
{
    DockSurfaceGeometry geometry;
    bool invoked = false;
    const QVariantMap bottomAtCorner = tooltipPlacement(
        geometry, 800, 600, QRectF(0, 500, 48, 48), 180, 34,
        QStringLiteral("bottom"), &invoked);
    QVERIFY2(invoked, "DockSurfaceGeometry must expose tooltip placement to QML");
    const QRect bottomRect = bottomAtCorner.value(QStringLiteral("cardRect")).toRect();
    QCOMPARE(bottomRect, QRect(8, 450, 180, 34));
    QCOMPARE(bottomAtCorner.value(QStringLiteral("pointerCenter")).toInt(), 16);

    const QVariantMap bottomFlipped = tooltipPlacement(
        geometry, 800, 600, QRectF(376, 2, 48, 48), 180, 34,
        QStringLiteral("bottom"), &invoked);
    QVERIFY(invoked);
    QCOMPARE(bottomFlipped.value(QStringLiteral("pointerEdge")).toString(), QStringLiteral("top"));
    QCOMPARE(bottomFlipped.value(QStringLiteral("cardRect")).toRect(), QRect(310, 66, 180, 34));

    const QVariantMap leftClamped = tooltipPlacement(
        geometry, 800, 600, QRectF(10, 560, 48, 48), 180, 34,
        QStringLiteral("left"), &invoked);
    QVERIFY(invoked);
    const QRect leftRect = leftClamped.value(QStringLiteral("cardRect")).toRect();
    QCOMPARE(leftRect, QRect(74, 558, 180, 34));
    QCOMPARE(leftClamped.value(QStringLiteral("pointerCenter")).toInt(), 20);
    QVERIFY(leftRect.left() >= 0 && leftRect.right() < 800);
    QVERIFY(leftRect.top() >= 0 && leftRect.bottom() < 600);

    const QVariantMap leftFlipped = tooltipPlacement(
        geometry, 800, 600, QRectF(740, 250, 48, 48), 180, 34,
        QStringLiteral("left"), &invoked);
    QVERIFY(invoked);
    QCOMPARE(leftFlipped.value(QStringLiteral("cardRect")).toRect(), QRect(544, 257, 180, 34));
    QCOMPARE(leftFlipped.value(QStringLiteral("pointerEdge")).toString(), QStringLiteral("right"));

    const QVariantMap rightFlipped = tooltipPlacement(
        geometry, 800, 600, QRectF(10, 250, 48, 48), 180, 34,
        QStringLiteral("right"), &invoked);
    QVERIFY(invoked);
    QCOMPARE(rightFlipped.value(QStringLiteral("cardRect")).toRect(), QRect(74, 257, 180, 34));
    QCOMPARE(rightFlipped.value(QStringLiteral("pointerEdge")).toString(), QStringLiteral("left"));
}

QTEST_GUILESS_MAIN(DockSurfaceGeometryTest)
#include "DockSurfaceGeometryTest.moc"
