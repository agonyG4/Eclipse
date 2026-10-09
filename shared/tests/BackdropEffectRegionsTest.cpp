#include "platform/wayland/effects/AstreaBackdropRegion.hpp"
#include "platform/wayland/effects/AstreaBackdropEffectRegions.hpp"
#include "platform/wayland/effects/AstreaBackdropAnalyticCoverage.hpp"
#include "platform/wayland/effects/RoundedEffectRegion.hpp"

#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickTransform>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>

#include <algorithm>

namespace {

class TestBackdropEffectRegions final : public AstreaBackdropEffectRegions {
public:
    using AstreaBackdropEffectRegions::AstreaBackdropEffectRegions;

    void completeForTest() { componentComplete(); }
};

void drainEvents()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

bool covered(const QVector<QRect> &rectangles, const QPoint point)
{
    return std::any_of(rectangles.cbegin(), rectangles.cend(), [point](const QRect &rectangle) {
        return rectangle.contains(point);
    });
}

} // namespace

class BackdropEffectRegionsTest final : public QObject {
    Q_OBJECT

private slots:
    void regionPropertiesAreStableAndBounded()
    {
        AstreaBackdropRegion region;

        QVERIFY(region.enabled());
        QCOMPARE(region.radius(), 0.0);
        region.setRadius(-10.0);
        QCOMPARE(region.radius(), 0.0);
        region.setRadius(12.5);
        QCOMPARE(region.radius(), 12.5);
        region.setEnabled(false);
        QVERIFY(!region.enabled());
    }

    void explicitLocalRectanglesResolveToTheExactUnion()
    {
        QQuickWindow window;
        window.resize(220, 120);
        window.show();
        auto *descriptors = new TestBackdropEffectRegions(window.contentItem());
        auto *item = new QQuickItem(window.contentItem());
        item->setSize({120.0, 60.0});
        item->setPosition({30.0, 20.0});

        AstreaBackdropRegion region(descriptors);
        region.setItem(item);
        const QVariantList localRectangles{
            QVariant::fromValue(QRectF(10.0, 10.0, 18.0, 12.0)),
            QVariant::fromValue(QRectF(64.0, 30.0, 16.0, 12.0)),
        };
        QVERIFY(region.metaObject()->indexOfProperty("localRectangles") >= 0);
        QVERIFY(region.setProperty("localRectangles", localRectangles));
        auto list = descriptors->regions();
        list.append(&list, &region);
        descriptors->completeForTest();
        drainEvents();

        const auto rectangles = descriptors->resolvedRegion();
        QVERIFY(covered(rectangles, {45, 36}));
        QVERIFY(covered(rectangles, {102, 55}));
        QVERIFY2(!covered(rectangles, {60, 24}),
                 "an explicit descriptor must not blur the rest of its item bounds");
    }

    void analyticShapeMapsToSurfaceLocalCoordinatesAndGetsOneGuardedWorkRect()
    {
        QQuickWindow window;
        window.resize(200, 120);
        window.show();
        auto *descriptors = new TestBackdropEffectRegions(window.contentItem());
        auto *item = new QQuickItem(window.contentItem());
        item->setSize({74.0, 44.0});
        item->setPosition({30.0, 35.0});
        auto *coverage = new AstreaBackdropAnalyticCoverage(descriptors);
        coverage->setItem(item);
        coverage->setRoundedRectEnabled(true);
        coverage->setRoundedRect(QRectF(1.0, 1.0, 72.0, 34.0));
        coverage->setRadius(17.0);
        coverage->setTriangleEnabled(true);
        coverage->setTriangleA({30.0, 35.0});
        coverage->setTriangleB({44.0, 35.0});
        coverage->setTriangleC({37.0, 43.0});
        descriptors->setAnalyticCoverage(coverage);

        AstreaBackdropRegion fallback(descriptors);
        fallback.setItem(item);
        fallback.setProperty("localRectangles", QVariantList{
            QVariant::fromValue(QRectF(1.0, 1.0, 72.0, 34.0)),
            QVariant::fromValue(QRectF(30.0, 35.0, 14.0, 1.0)),
            QVariant::fromValue(QRectF(31.0, 36.0, 12.0, 1.0)),
            QVariant::fromValue(QRectF(32.0, 37.0, 10.0, 1.0)),
            QVariant::fromValue(QRectF(33.0, 38.0, 8.0, 1.0)),
            QVariant::fromValue(QRectF(34.0, 39.0, 6.0, 1.0)),
            QVariant::fromValue(QRectF(35.0, 40.0, 4.0, 1.0)),
            QVariant::fromValue(QRectF(36.0, 41.0, 2.0, 1.0)),
        });
        auto list = descriptors->regions();
        list.append(&list, &fallback);
        descriptors->completeForTest();
        drainEvents();

        const auto resolved = descriptors->resolvedAnalyticCoverage();
        QVERIFY(resolved.has_value());
        QCOMPARE(resolved->roundedRect->rect, QRectF(31.0, 36.0, 72.0, 34.0));
        QCOMPARE(resolved->roundedRect->radius, 17.0);
        QCOMPARE(resolved->triangle->a, QPointF(60.0, 70.0));
        QCOMPARE(resolved->triangle->b, QPointF(74.0, 70.0));
        QCOMPARE(resolved->triangle->c, QPointF(67.0, 78.0));

        const auto work = descriptors->resolvedAnalyticWorkRegion();
        QCOMPARE(work.size(), 1);
        QCOMPARE(work.constFirst(), QRect(29, 34, 76, 46));
        QCOMPARE(descriptors->resolvedRegion().size(), 8);
        QCOMPARE(AstreaBackdropEffectRegions::analyticCoverageGuard, 2);
    }

    void analyticBoundsClipAtSurfaceButShapeCoordinatesDoNot()
    {
        const AstreaAnalyticBackdropCoverage coverage{
            AstreaAnalyticRoundedRect{QRectF(188.5, 12.25, 72.0, 34.0), 17.0},
            AstreaAnalyticTriangle{QPointF(218.5, 46.25), QPointF(232.5, 46.25),
                                   QPointF(225.5, 54.25)}};
        const auto work = AstreaBackdropEffectRegions::analyticWorkRegion(coverage,
                                                                           QSize(200, 100));
        QCOMPARE(work.size(), 1);
        QCOMPARE(work.constFirst(), QRect(186, 10, 14, 47));
        QCOMPARE(coverage.roundedRect->rect.x(), 188.5);
        QCOMPARE(coverage.triangle->c, QPointF(225.5, 54.25));
    }

    void analyticRoundedRectRejectsRotationAndNonUniformScale()
    {
        QQuickWindow window;
        window.resize(320, 200);
        window.show();
        auto *parent = new QQuickItem(window.contentItem());
        parent->setPosition({14.0, 18.0});
        auto *descriptors = new TestBackdropEffectRegions(parent);
        auto *item = new QQuickItem(parent);
        item->setPosition({24.0, 30.0});
        item->setSize({72.0, 34.0});
        auto *coverage = new AstreaBackdropAnalyticCoverage(descriptors);
        coverage->setItem(item);
        coverage->setRoundedRectEnabled(true);
        coverage->setRoundedRect(QRectF(0.0, 0.0, 72.0, 34.0));
        coverage->setRadius(17.0);
        coverage->setTriangleEnabled(true);
        coverage->setTriangleA({29.0, 34.0});
        coverage->setTriangleB({43.0, 34.0});
        coverage->setTriangleC({36.0, 42.0});
        descriptors->setAnalyticCoverage(coverage);

        AstreaBackdropRegion fallback(descriptors);
        fallback.setItem(item);
        auto list = descriptors->regions();
        list.append(&list, &fallback);
        descriptors->completeForTest();
        drainEvents();

        parent->setScale(1.5);
        auto uniform = descriptors->resolvedAnalyticCoverage();
        QVERIFY(uniform.has_value());
        QCOMPARE(uniform->roundedRect->rect.size(), QSizeF(108.0, 51.0));
        QCOMPARE(uniform->roundedRect->radius, 25.5);

        parent->setRotation(17.0);
        QVERIFY(!descriptors->resolvedAnalyticCoverage().has_value());
        QVERIFY(!descriptors->resolvedRegion().isEmpty());

        parent->setRotation(0.0);
        parent->setScale(1.0);
        QQmlEngine engine;
        QQmlComponent scaleComponent(&engine);
        scaleComponent.setData("import QtQuick\nScale { xScale: 1.5; yScale: 1.0 }", QUrl());
        QVERIFY2(scaleComponent.isReady(), qPrintable(scaleComponent.errorString()));
        auto *nonUniform = qobject_cast<QQuickTransform *>(scaleComponent.create());
        QVERIFY2(nonUniform, qPrintable(scaleComponent.errorString()));
        auto transforms = parent->transform();
        transforms.append(&transforms, nonUniform);
        QVERIFY(!descriptors->resolvedAnalyticCoverage().has_value());
        QVERIFY(!descriptors->resolvedRegion().isEmpty());
    }

    void explicitLocalRectanglesFollowOwnerAndAncestorTransforms()
    {
        QQuickWindow window;
        window.resize(360, 240);
        window.show();
        auto *parent = new QQuickItem(window.contentItem());
        parent->setSize({220.0, 150.0});
        parent->setPosition({15.0, 18.0});
        auto *descriptors = new TestBackdropEffectRegions(parent);
        auto *item = new QQuickItem(parent);
        item->setSize({120.0, 100.0});
        item->setPosition({20.0, 25.0});
        const QRectF localRectangle(12.0, 14.0, 28.0, 22.0);
        AstreaBackdropRegion region(descriptors);
        region.setItem(item);
        QVERIFY(region.setProperty("localRectangles", QVariantList{
                                       QVariant::fromValue(localRectangle)}));
        auto list = descriptors->regions();
        list.append(&list, &region);
        descriptors->completeForTest();
        drainEvents();

        const auto assertMappedCenterIsCovered = [&] {
            const QPoint point = item->mapToItem(window.contentItem(),
                                                 localRectangle.center()).toPoint();
            QVERIFY(covered(descriptors->resolvedRegion(), point));
        };
        assertMappedCenterIsCovered();
        QVERIFY(!covered(descriptors->resolvedRegion(),
                         item->mapToItem(window.contentItem(), QPointF(90.0, 80.0)).toPoint()));

        item->setPosition({55.0, 45.0});
        parent->setScale(1.35);
        QMetaObject::invokeMethod(&window, "afterAnimating", Qt::DirectConnection);
        drainEvents();
        assertMappedCenterIsCovered();
        QVERIFY(!covered(descriptors->resolvedRegion(),
                         item->mapToItem(window.contentItem(), QPointF(90.0, 80.0)).toPoint()));
    }

    void emptyExplicitLocalRectanglesKeepWholeItemRoundedBehavior()
    {
        QQuickWindow window;
        window.resize(180, 100);
        window.show();
        auto *descriptors = new TestBackdropEffectRegions(window.contentItem());
        auto *item = new QQuickItem(window.contentItem());
        item->setSize({88.0, 36.0});
        item->setPosition({20.0, 24.0});
        AstreaBackdropRegion region(descriptors);
        region.setItem(item);
        region.setRadius(12.0);
        QVERIFY(region.setProperty("localRectangles", QVariantList{}));
        auto list = descriptors->regions();
        list.append(&list, &region);
        descriptors->completeForTest();
        drainEvents();

        auto expected = AstreaRoundedEffectRegion::rectangles(QSize(88, 36), 12.0);
        for (auto &rectangle : expected)
            rectangle.translate(20, 24);
        QCOMPARE(descriptors->resolvedRegion(), expected);
        QVERIFY(covered(expected, {64, 42}));
        QVERIFY(!covered(expected, {20, 24}));
    }

    void explicitLocalRectanglesShareTheAggregate96RectangleBudget()
    {
        QQuickWindow window;
        window.resize(500, 200);
        window.show();
        auto *descriptors = new TestBackdropEffectRegions(window.contentItem());

        auto *body = new QQuickItem(window.contentItem());
        body->setSize({100.0, 60.0});
        body->setPosition({10.0, 100.0});
        auto *bodyRegion = new AstreaBackdropRegion(descriptors);
        bodyRegion->setItem(body);
        bodyRegion->setRadius(30.0);
        auto list = descriptors->regions();
        list.append(&list, bodyRegion);

        auto *tail = new QQuickItem(window.contentItem());
        tail->setSize({490.0, 40.0});
        tail->setPosition({0.0, 0.0});
        auto *tailRegion = new AstreaBackdropRegion(descriptors);
        tailRegion->setItem(tail);
        QVariantList localRectangles;
        for (int index = 0; index < 70; ++index)
            localRectangles.append(QVariant::fromValue(QRectF(2.0 + 4.0 * index,
                                                               20.0, 1.0, 1.0)));
        QVERIFY(tailRegion->setProperty("localRectangles", localRectangles));
        list.append(&list, tailRegion);

        descriptors->completeForTest();
        drainEvents();

        const auto rectangles = descriptors->resolvedRegion();
        QVERIFY2(rectangles.size() <= 96,
                 qPrintable(QStringLiteral("resolved %1 rectangles").arg(rectangles.size())));
        QVERIFY(rectangles.size() >= 70);
        QVERIFY(covered(rectangles, {2, 20}));
        QVERIFY(covered(rectangles, {278, 20}));
        QVERIFY(covered(rectangles, {60, 130}));
    }

    void hiddenWindowResynchronizesWhenShown()
    {
        QQuickWindow window;
        window.resize(320, 180);

        auto *descriptors = new TestBackdropEffectRegions(window.contentItem());
        auto *item = new QQuickItem(window.contentItem());
        item->setSize({120.0, 60.0});
        item->setPosition({20.0, 30.0});
        auto *region = new AstreaBackdropRegion(descriptors);
        region->setItem(item);
        auto list = descriptors->regions();
        list.append(&list, region);
        descriptors->completeForTest();

        QVERIFY(!window.isVisible());
        drainEvents();
        QVERIFY(descriptors->resolvedRegion().isEmpty());

        window.show();
        drainEvents();
        QVERIFY(!descriptors->resolvedRegion().isEmpty());
    }

    void ancestorTransformAndPlacementResynchronizeOnAnimationTick()
    {
        QQuickWindow window;
        window.resize(320, 180);
        window.show();

        auto *parent = new QQuickItem(window.contentItem());
        parent->setPosition({10.0, 15.0});
        parent->setScale(1.0);
        auto *descriptors = new TestBackdropEffectRegions(parent);
        auto *item = new QQuickItem(parent);
        item->setSize({100.0, 40.0});
        item->setPosition({20.0, 25.0});
        auto *region = new AstreaBackdropRegion(descriptors);
        region->setItem(item);
        auto list = descriptors->regions();
        list.append(&list, region);
        descriptors->completeForTest();
        drainEvents();

        const auto initial = descriptors->resolvedRegion();
        QVERIFY(!initial.isEmpty());

        parent->setScale(1.5);
        QMetaObject::invokeMethod(&window, "afterAnimating", Qt::DirectConnection);
        drainEvents();
        const auto scaled = descriptors->resolvedRegion();
        QVERIFY(!scaled.isEmpty());
        QVERIFY(scaled != initial);

        parent->setPosition({80.0, 45.0});
        QMetaObject::invokeMethod(&window, "afterAnimating", Qt::DirectConnection);
        drainEvents();
        QVERIFY(descriptors->resolvedRegion() != scaled);
    }

    void animationTickSynchronizesImmediatelyAndDeduplicates()
    {
        QQuickWindow window;
        window.resize(320, 180);
        window.show();

        auto *parent = new QQuickItem(window.contentItem());
        auto *descriptors = new TestBackdropEffectRegions(parent);
        auto *item = new QQuickItem(parent);
        item->setSize({100.0, 40.0});
        item->setPosition({20.0, 25.0});
        auto *region = new AstreaBackdropRegion(descriptors);
        region->setItem(item);
        auto list = descriptors->regions();
        list.append(&list, region);
        descriptors->completeForTest();
        drainEvents();

        QSignalSpy changes(descriptors, &AstreaBackdropEffectRegions::resolvedRegionChanged);
        const auto initial = descriptors->resolvedRegion();

        parent->setScale(1.5);
        QMetaObject::invokeMethod(&window, "afterAnimating", Qt::DirectConnection);
        QCOMPARE(changes.count(), 1);
        QVERIFY(descriptors->resolvedRegion() != initial);

        changes.clear();
        QMetaObject::invokeMethod(&window, "afterAnimating", Qt::DirectConnection);
        QCOMPARE(changes.count(), 0);

        parent->setPosition({80.0, 45.0});
        QMetaObject::invokeMethod(&window, "afterAnimating", Qt::DirectConnection);
        QCOMPARE(changes.count(), 1);
    }

    void aggregateRegionBudgetPreservesSeparateHighRadiusCards()
    {
        QQuickWindow window;
        window.resize(320, 280);
        window.show();
        auto *descriptors = new TestBackdropEffectRegions(window.contentItem());
        const QVector<QPoint> positions = {
            {10, 10}, {170, 10}, {10, 160}, {170, 160},
        };
        for (const auto position : positions) {
            auto *item = new QQuickItem(window.contentItem());
            item->setSize({120.0, 100.0});
            item->setPosition(position);
            auto *region = new AstreaBackdropRegion(descriptors);
            region->setItem(item);
            region->setRadius(40.0);
            auto list = descriptors->regions();
            list.append(&list, region);
        }
        descriptors->completeForTest();
        drainEvents();

        const auto rectangles = descriptors->resolvedRegion();
        QVERIFY(rectangles.size() <= 96);
        for (const auto position : positions)
            QVERIFY(covered(rectangles, position + QPoint(60, 50)));
        QVERIFY(!covered(rectangles, {150, 60}));
        QVERIFY(!covered(rectangles, {150, 210}));
        QVERIFY(!covered(rectangles, {70, 135}));
    }

    void roundedPillPreservesCornerCutout()
    {
        QQuickWindow window;
        window.resize(140, 80);
        window.show();
        auto *descriptors = new TestBackdropEffectRegions(window.contentItem());
        auto *item = new QQuickItem(window.contentItem());
        item->setSize({88.0, 36.0});
        item->setPosition({20.0, 20.0});
        auto *region = new AstreaBackdropRegion(descriptors);
        region->setItem(item);
        region->setRadius(12.0);
        auto list = descriptors->regions();
        list.append(&list, region);
        descriptors->completeForTest();
        drainEvents();

        const auto rectangles = descriptors->resolvedRegion();
        QVERIFY(rectangles.size() > 1);
        QVERIFY(covered(rectangles, {64, 38}));
        QVERIFY(!covered(rectangles, {20, 20}));
    }

    void aggregateRegionBudgetRefusesUnrepresentableLayout()
    {
        QQuickWindow window;
        window.resize(800, 380);
        window.show();
        auto *descriptors = new TestBackdropEffectRegions(window.contentItem());

        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 11; ++column) {
                auto *item = new QQuickItem(window.contentItem());
                item->setSize({50.0, 100.0});
                item->setPosition({10.0 + column * 70.0, 10.0 + row * 120.0});
                auto *region = new AstreaBackdropRegion(descriptors);
                region->setItem(item);
                region->setRadius(25.0);
                auto list = descriptors->regions();
                list.append(&list, region);
            }
        }

        descriptors->completeForTest();
        drainEvents();

        const auto rectangles = descriptors->resolvedRegion();
        QVERIFY(rectangles.size() <= 96);
        QVERIFY(rectangles.isEmpty());
    }

    void destroyedDynamicItemBecomesNoRegion()
    {
        QQuickWindow window;
        window.resize(320, 180);
        window.show();
        auto *descriptors = new TestBackdropEffectRegions(window.contentItem());
        auto *item = new QQuickItem(window.contentItem());
        item->setSize({80.0, 40.0});
        auto *region = new AstreaBackdropRegion(descriptors);
        region->setItem(item);
        auto list = descriptors->regions();
        list.append(&list, region);
        descriptors->completeForTest();
        drainEvents();
        QVERIFY(!descriptors->resolvedRegion().isEmpty());

        delete item;
        drainEvents();
        QVERIFY(descriptors->resolvedRegion().isEmpty());
    }
};

QTEST_MAIN(BackdropEffectRegionsTest)
#include "BackdropEffectRegionsTest.moc"
