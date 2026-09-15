#include "platform/wayland/effects/AstreaBackdropRegion.hpp"
#include "platform/wayland/effects/AstreaBackdropEffectRegions.hpp"

#include <QCoreApplication>
#include <QQuickItem>
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
