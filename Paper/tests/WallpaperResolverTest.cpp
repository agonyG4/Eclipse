#include "core/WallpaperResolver.hpp"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace Paper;

class WallpaperResolverTest final : public QObject
{
    Q_OBJECT

private slots:
    void resolvesImagePathWithSpacesAndUnicode();
    void resolvesFileUriAndSymlinkTarget();
    void rejectsMissingDirectoryAndUnsupportedImage();
    void rejectsUnsupportedKindAndScopeBeforeFilesystemAccess();
    void usesExplicitEmergencyFallbackWhenDefaultIsUnavailable();
    void defaultResolverProvidesPackagedFactoryArtwork();

private:
    static QString writeImage(const QString &path);
};

QString WallpaperResolverTest::writeImage(const QString &path)
{
    QImage image(16, 12, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    if (!image.save(path)) {
        qFatal("Could not create test image at %s", qPrintable(path));
    }
    return path;
}

void WallpaperResolverTest::resolvesImagePathWithSpacesAndUnicode()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto path = writeImage(temp.filePath(QStringLiteral("snow & café.png")));

    const auto result = WallpaperResolver().resolve(
        WallpaperDescriptor::externalFile(path, WallpaperFit::Contain));

    QVERIFY2(result.ok(), qPrintable(result.message));
    QCOMPARE(result.descriptor.resolvedSource(), QFileInfo(path).canonicalFilePath());
    QCOMPARE(result.descriptor.fit(), WallpaperFit::Contain);
}

void WallpaperResolverTest::resolvesFileUriAndSymlinkTarget()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto imagePath = writeImage(temp.filePath(QStringLiteral("wallpaper.png")));
    const auto linkPath = temp.filePath(QStringLiteral("wallpaper-link.png"));
    QVERIFY(QFile::link(imagePath, linkPath));

    const auto result = WallpaperResolver().resolve(
        WallpaperDescriptor::externalFile(QUrl::fromLocalFile(linkPath).toString(),
                                           WallpaperFit::Cover));

    QVERIFY2(result.ok(), qPrintable(result.message));
    QCOMPARE(result.descriptor.resolvedSource(), QFileInfo(imagePath).canonicalFilePath());
}

void WallpaperResolverTest::rejectsMissingDirectoryAndUnsupportedImage()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const auto missing = WallpaperResolver().resolve(WallpaperDescriptor::externalFile(
        temp.filePath(QStringLiteral("missing.png")), WallpaperFit::Cover));
    QCOMPARE(missing.error, WallpaperResolutionError::SourceMissing);

    const auto directory = WallpaperResolver().resolve(
        WallpaperDescriptor::externalFile(temp.path(), WallpaperFit::Cover));
    QCOMPARE(directory.error, WallpaperResolutionError::SourceNotRegularFile);

    const auto textPath = temp.filePath(QStringLiteral("not-an-image.txt"));
    QFile text(textPath);
    QVERIFY(text.open(QIODevice::WriteOnly));
    QVERIFY(text.write("not an image") > 0);
    text.close();
    const auto unsupported = WallpaperResolver().resolve(
        WallpaperDescriptor::externalFile(textPath, WallpaperFit::Cover));
    QCOMPARE(unsupported.error, WallpaperResolutionError::UnsupportedImage);
}

void WallpaperResolverTest::rejectsUnsupportedKindAndScopeBeforeFilesystemAccess()
{
    auto descriptor = WallpaperDescriptor::externalFile(
        QStringLiteral("/path/that/does/not/exist"), WallpaperFit::Cover);
    descriptor.setKind(WallpaperKind::Video);
    const auto unsupportedKind = WallpaperResolver().resolve(descriptor);
    QCOMPARE(unsupportedKind.error, WallpaperResolutionError::UnsupportedKind);

    descriptor.setKind(WallpaperKind::Image);
    descriptor.setScope(WallpaperScope::Output);
    const auto unsupportedScope = WallpaperResolver().resolve(descriptor);
    QCOMPARE(unsupportedScope.error, WallpaperResolutionError::UnsupportedScope);
}

void WallpaperResolverTest::usesExplicitEmergencyFallbackWhenDefaultIsUnavailable()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));

    const auto result = WallpaperResolver(
        temp.filePath(QStringLiteral("missing-factory.png")), emergency)
                             .factoryDefault(WallpaperFit::Center);

    QVERIFY2(result.ok(), qPrintable(result.message));
    QCOMPARE(result.error, WallpaperResolutionError::EmergencyFallback);
    QCOMPARE(result.descriptor.logicalId(), QStringLiteral("astrea://wallpaper/emergency"));
    QCOMPARE(result.descriptor.fit(), WallpaperFit::Center);
}

void WallpaperResolverTest::defaultResolverProvidesPackagedFactoryArtwork()
{
    qunsetenv("ASTREA_WALLPAPER_DEFAULT");
    qunsetenv("ASTREA_ROOT");
    const auto result = WallpaperResolver().factoryDefault();

    QVERIFY2(result.ok(), qPrintable(result.message));
    QCOMPARE(result.error, WallpaperResolutionError::None);
    QCOMPARE(result.descriptor.logicalId(), QStringLiteral("astrea://wallpaper/default"));
    QVERIFY(result.descriptor.resolvedSource().endsWith(QStringLiteral("default.jpg")));
    QVERIFY(!result.descriptor.resolvedSource().startsWith(QStringLiteral("qrc:")));
    QVERIFY(!result.descriptor.resolvedSource().startsWith(QStringLiteral(":/")));
    QVERIFY(QFileInfo(result.descriptor.resolvedSource()).isAbsolute());
    QVERIFY(QFileInfo::exists(result.descriptor.resolvedSource()));
}

QTEST_MAIN(WallpaperResolverTest)
#include "WallpaperResolverTest.moc"
