#include "platform/wayland/effects/AstreaEffectSurfaceController.hpp"
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
        QVERIFY(state.recordRegionSync(first, true));
        QCOMPARE(state.qtFrameRequests(), quint64(1));
        QVERIFY(!state.recordRegionSync(first, true));
        QCOMPARE(state.qtFrameRequests(), quint64(1));
        QVERIFY(state.recordRegionSync(second, true));
        QCOMPARE(state.qtFrameRequests(), quint64(2));
        QVERIFY(state.clearRegion(true));
        QCOMPARE(state.qtFrameRequests(), quint64(3));
        QVERIFY(!state.clearRegion(true));
        QCOMPARE(state.qtFrameRequests(), quint64(3));
    }
};

QTEST_APPLESS_MAIN(WaylandEffectsStateTest)
#include "WaylandEffectsStateTest.moc"
