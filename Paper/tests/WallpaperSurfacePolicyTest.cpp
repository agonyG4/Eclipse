#include "platform/wayland/WallpaperSurfacePolicy.hpp"

#include <QGuiApplication>
#include <QMargins>
#include <QScreen>
#include <QTest>

using namespace Paper;

class WallpaperSurfacePolicyTest final : public QObject
{
    Q_OBJECT

private slots:
    void returnsFullOutputBackgroundContract();
};

void WallpaperSurfacePolicyTest::returnsFullOutputBackgroundContract()
{
    auto *screen = QGuiApplication::primaryScreen();
    QVERIFY(screen);

    const auto config = WallpaperSurfacePolicy::background(screen);
    QCOMPARE(config.scope, QStringLiteral("astrea-paper-wallpaper"));
    QCOMPARE(config.layer, AstreaLayerShellConfig::Layer::Background);
    QCOMPARE(config.keyboardInteractivity,
             AstreaLayerShellConfig::KeyboardInteractivity::None);
    QVERIFY(config.anchorTop);
    QVERIFY(config.anchorBottom);
    QVERIFY(config.anchorLeft);
    QVERIFY(config.anchorRight);
    QCOMPARE(config.exclusiveZone, -1);
    QCOMPARE(config.margins, QMargins());
    QCOMPARE(config.screen, screen);
}

QTEST_MAIN(WallpaperSurfacePolicyTest)
#include "WallpaperSurfacePolicyTest.moc"
