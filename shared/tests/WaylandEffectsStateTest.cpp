#include "platform/wayland/effects/AstreaEffectSurfaceController.hpp"
#include "platform/wayland/effects/AstreaBackgroundEffectBinding.hpp"
#include "platform/wayland/effects/AstreaWaylandEffects.hpp"

#include <QTest>

class WaylandEffectsStateTest final : public QObject {
    Q_OBJECT

private slots:
    void capabilityDeliveryIsSeparateFromManagerBinding()
    {
        AstreaWaylandEffectsCapabilityState state;

        QVERIFY(!state.managerBound());
        QVERIFY(!state.available());
        QVERIFY(!state.setManagerBound(true));
        QVERIFY(!state.available());
        QVERIFY(state.setCapabilities(AstreaWaylandEffectsCapabilityState::BlurCapability));
        QVERIFY(state.available());
    }

    void capabilityLossAndReturnUpdatesAvailability()
    {
        AstreaWaylandEffectsCapabilityState state;
        state.setManagerBound(true);
        state.setCapabilities(AstreaWaylandEffectsCapabilityState::BlurCapability);
        QVERIFY(state.available());

        QVERIFY(state.setCapabilities(0));
        QVERIFY(!state.available());
        QVERIFY(state.setCapabilities(AstreaWaylandEffectsCapabilityState::BlurCapability));
        QVERIFY(state.available());
    }

    void privateCapabilityRequiresAuthorizationOnTheCurrentDisplay()
    {
        AstreaWaylandEffectsCapabilityState state;
        state.setManagerBound(true);
        state.setCapabilities(AstreaWaylandEffectsCapabilityState::BlurCapability);
        QVERIFY(state.available());

        QVERIFY(!AstreaWaylandEffectsCapabilityState::analyticCoverageAvailable(
            true, true, false, 3, 0));
        QVERIFY(!AstreaWaylandEffectsCapabilityState::analyticCoverageAvailable(
            false, true, true, 3, 3));
        QVERIFY(AstreaWaylandEffectsCapabilityState::analyticCoverageAvailable(
            true, true, true, 3, 3));
        QVERIFY(!AstreaWaylandEffectsCapabilityState::analyticCoverageAvailable(
            true, true, true, 4, 3));
        QVERIFY(state.available());
    }

    void nativeSurfaceGenerationsOwnAtMostOneEffect()
    {
        AstreaEffectSurfaceLifecycleState state;

        QVERIFY(state.surfaceCreated(1));
        QCOMPARE(state.generation(), quint64(1));
        QVERIFY(state.bindEffect());
        QVERIFY(!state.bindEffect());
        QCOMPARE(state.totalEffectBindings(), quint64(1));

        QVERIFY(state.surfaceAboutToBeDestroyed(1));
        QVERIFY(!state.hasSurface());
        QVERIFY(!state.hasEffect());
        QVERIFY(state.surfaceCreated(2));
        QCOMPARE(state.generation(), quint64(2));
        QVERIFY(state.bindEffect());
        QCOMPARE(state.totalEffectBindings(), quint64(2));

        state.destroyEffect();
        QVERIFY(!state.hasEffect());
        QVERIFY(state.surfaceAboutToBeDestroyed(2));
        QVERIFY(!state.hasSurface());
    }

    void changedRegionRequestsOneQtFrameAndUnchangedRegionRequestsNone()
    {
        AstreaEffectSurfaceLifecycleState state;
        const QVector<QRect> first{{10, 20, 40, 30}};
        const QVector<QRect> second{{12, 20, 40, 30}};

        QVERIFY(state.surfaceCreated(1));
        QVERIFY(state.bindEffect());
        QVERIFY(state.recordSync(AstreaBackgroundEffectMode::Fallback, first,
                                std::nullopt, true));
        QCOMPARE(state.qtFrameRequests(), quint64(1));
        QVERIFY(!state.recordSync(AstreaBackgroundEffectMode::Fallback, first,
                                 std::nullopt, true));
        QCOMPARE(state.qtFrameRequests(), quint64(1));
        QVERIFY(state.recordSync(AstreaBackgroundEffectMode::Fallback, second,
                                std::nullopt, true));
        QCOMPARE(state.qtFrameRequests(), quint64(2));
        QVERIFY(state.recordSync(AstreaBackgroundEffectMode::Disabled, {},
                                std::nullopt, true));
        QCOMPARE(state.qtFrameRequests(), quint64(3));
        QVERIFY(!state.recordSync(AstreaBackgroundEffectMode::Disabled, {},
                                 std::nullopt, true));
        QCOMPARE(state.qtFrameRequests(), quint64(3));
    }

    void publicAndPrivateCoverageTransitionsShareOneFramePerState()
    {
        AstreaBackgroundEffectLifecycleState state;
        const QVector<QRect> fallback{{18, 24, 72, 34}, {40, 58, 8, 6}};
        const QVector<QRect> coarse{{16, 22, 76, 46}};
        const AstreaAnalyticBackdropCoverage analytic{
            AstreaAnalyticRoundedRect{QRectF(18, 24, 72, 34), 17.0},
            AstreaAnalyticTriangle{QPointF(47, 58), QPointF(61, 58), QPointF(54, 66)}};

        QVERIFY(state.surfaceCreated(1));
        QVERIFY(state.bindEffect());
        QVERIFY(state.bindCoverage());
        QCOMPARE(state.totalEffectBindings(), quint64(1));
        QCOMPARE(state.totalCoverageBindings(), quint64(1));

        QVERIFY(state.recordSync(AstreaBackgroundEffectMode::Fallback, fallback,
                                std::nullopt, true));
        QCOMPARE(state.qtFrameRequests(), quint64(1));
        QVERIFY(!state.recordSync(AstreaBackgroundEffectMode::Fallback, fallback,
                                 std::nullopt, true));
        QCOMPARE(state.qtFrameRequests(), quint64(1));

        QVERIFY(state.recordSync(AstreaBackgroundEffectMode::Analytic, coarse, analytic, true));
        QCOMPARE(state.qtFrameRequests(), quint64(2));
        auto movedTriangle = analytic;
        movedTriangle.triangle->c += QPointF(1.0, 0.0);
        QVERIFY(state.recordSync(AstreaBackgroundEffectMode::Analytic, coarse,
                                movedTriangle, true));
        QCOMPARE(state.qtFrameRequests(), quint64(3));
        const QVector<QRect> movedWorkRegion{{17, 22, 76, 46}};
        QVERIFY(state.recordSync(AstreaBackgroundEffectMode::Analytic, movedWorkRegion,
                                movedTriangle, true));
        QCOMPARE(state.qtFrameRequests(), quint64(4));

        QVERIFY(state.recordSync(AstreaBackgroundEffectMode::Fallback, fallback,
                                std::nullopt, true));
        QCOMPARE(state.qtFrameRequests(), quint64(5));
        QVERIFY(state.recordSync(AstreaBackgroundEffectMode::Analytic, coarse, analytic, true));
        QCOMPARE(state.qtFrameRequests(), quint64(6));
        QVERIFY(state.recordSync(AstreaBackgroundEffectMode::Disabled, {},
                                std::nullopt, true));
        QCOMPARE(state.qtFrameRequests(), quint64(7));
        QCOMPARE(state.mode(), AstreaBackgroundEffectMode::Disabled);

        QVERIFY(state.surfaceAboutToBeDestroyed(1));
        QVERIFY(!state.hasEffect());
        QVERIFY(!state.hasCoverage());
        QVERIFY(state.surfaceCreated(2));
        QCOMPARE(state.generation(), quint64(2));
        QVERIFY(state.bindEffect());
        QVERIFY(state.bindCoverage());
        QCOMPARE(state.totalEffectBindings(), quint64(2));
        QCOMPARE(state.totalCoverageBindings(), quint64(2));
    }
};

QTEST_APPLESS_MAIN(WaylandEffectsStateTest)
#include "WaylandEffectsStateTest.moc"
