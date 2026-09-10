#include "platform/wayland/effects/AstreaBackdropRegion.hpp"

#include <QTest>

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
};

QTEST_APPLESS_MAIN(BackdropEffectRegionsTest)
#include "BackdropEffectRegionsTest.moc"
