#include "core/WallpaperDescriptor.hpp"

#include <QJsonObject>
#include <QTest>

using namespace Paper;

class WallpaperDescriptorTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsImageGlobalDescriptor();
    void rejectsUnsupportedKindAndScope();
    void roundTripsFitValues();
    void strictlyRejectsUnknownFitValues();
    void serializesStableSourceFields();
    void infersSystemOriginForLegacySystemJson();
};

void WallpaperDescriptorTest::acceptsImageGlobalDescriptor()
{
    const auto descriptor = WallpaperDescriptor::externalFile(
        QStringLiteral("/tmp/Astrea Wallpaper.png"), WallpaperFit::Cover);

    QVERIFY(descriptor.isValid());
    QCOMPARE(descriptor.kind(), WallpaperKind::Image);
    QCOMPARE(descriptor.sourceKind(), WallpaperSourceKind::ExternalFile);
    QCOMPARE(descriptor.fit(), WallpaperFit::Cover);
    QCOMPARE(descriptor.scope(), WallpaperScope::Global);
    QCOMPARE(descriptor.source(), QStringLiteral("/tmp/Astrea Wallpaper.png"));
}

void WallpaperDescriptorTest::rejectsUnsupportedKindAndScope()
{
    auto dynamic = WallpaperDescriptor::externalFile(
        QStringLiteral("/tmp/animated.webm"), WallpaperFit::Cover);
    dynamic.setKind(WallpaperKind::Video);
    QVERIFY(!dynamic.isValid());

    auto outputScoped = WallpaperDescriptor::externalFile(
        QStringLiteral("/tmp/wallpaper.png"), WallpaperFit::Cover);
    outputScoped.setScope(WallpaperScope::Output);
    QVERIFY(!outputScoped.isValid());
}

void WallpaperDescriptorTest::roundTripsFitValues()
{
    for (const auto fit : {WallpaperFit::Cover, WallpaperFit::Contain,
                           WallpaperFit::Stretch, WallpaperFit::Center,
                           WallpaperFit::Tile}) {
        const auto name = wallpaperFitToString(fit);
        QVERIFY(!name.isEmpty());
        QCOMPARE(wallpaperFitFromString(name), fit);
    }

    QCOMPARE(wallpaperFitFromString(QStringLiteral("unknown")),
             WallpaperFit::Cover);
}

void WallpaperDescriptorTest::strictlyRejectsUnknownFitValues()
{
    for (const auto fit : {WallpaperFit::Cover, WallpaperFit::Contain,
                           WallpaperFit::Stretch, WallpaperFit::Center,
                           WallpaperFit::Tile}) {
        const auto name = wallpaperFitToString(fit);
        const auto parsed = wallpaperFitFromStringStrict(name);
        QVERIFY(parsed.has_value());
        QCOMPARE(*parsed, fit);
    }
    QVERIFY(!wallpaperFitFromStringStrict(QStringLiteral("unknown")).has_value());
}

void WallpaperDescriptorTest::serializesStableSourceFields()
{
    auto descriptor = WallpaperDescriptor::externalFile(
        QStringLiteral("file:///tmp/space%20and%20snow.png"), WallpaperFit::Contain);
    descriptor.setLogicalId(QStringLiteral("file:wallpaper/test"));
    descriptor.setResolvedSource(QStringLiteral("/tmp/space and snow.png"));

    const QJsonObject json = descriptor.toJson();
    QCOMPARE(json.value(QStringLiteral("kind")).toString(), QStringLiteral("image"));
    QCOMPARE(json.value(QStringLiteral("sourceKind")).toString(), QStringLiteral("external-file"));
    QCOMPARE(json.value(QStringLiteral("source")).toString(),
             QStringLiteral("file:///tmp/space%20and%20snow.png"));
    QCOMPARE(json.value(QStringLiteral("logicalId")).toString(),
             QStringLiteral("file:wallpaper/test"));
    QCOMPARE(json.value(QStringLiteral("fit")).toString(), QStringLiteral("contain"));
    QCOMPARE(json.value(QStringLiteral("scope")).toString(), QStringLiteral("global"));
}

void WallpaperDescriptorTest::infersSystemOriginForLegacySystemJson()
{
    const auto descriptor = WallpaperDescriptor::fromJson(
        QJsonObject{{QStringLiteral("kind"), QStringLiteral("image")},
                    {QStringLiteral("sourceKind"), QStringLiteral("system-resource")},
                    {QStringLiteral("fit"), QStringLiteral("cover")},
                    {QStringLiteral("scope"), QStringLiteral("global")},
                    {QStringLiteral("logicalId"), QStringLiteral("astrea://wallpaper/default")},
                    {QStringLiteral("source"), QStringLiteral("/usr/share/default.jpg")} });
    QCOMPARE(descriptor.origin(), WallpaperOrigin::System);
}

QTEST_MAIN(WallpaperDescriptorTest)
#include "WallpaperDescriptorTest.moc"
