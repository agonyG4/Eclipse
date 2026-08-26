#include "core/WallpaperCatalog.hpp"

#include "core/WallpaperDescriptor.hpp"
#include "core/WallpaperResolver.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

using namespace Paper;

class WallpaperCatalogTest final : public QObject
{
    Q_OBJECT

private slots:
    void listsStableSystemDefault();
    void importsUserImageWithStableId();
    void reusesDuplicateContent();
    void persistsDisplayNameAcrossRefreshAndReconstruction();
    void updatesDuplicateDisplayNameWithoutSecondImage();
    void ignoresMalformedMetadataWithoutExposingDigest();
    void rejectsFailedMetadataPublicationWithoutCatalogEntry();
    void rejectsDirectoryAndUnsupportedImage();
};

namespace {

QString writeImage(const QString &path)
{
    QImage image(32, 24, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    if (!image.save(path)) {
        qFatal("Could not write test image at %s", qPrintable(path));
    }
    return path;
}

} // namespace

void WallpaperCatalogTest::listsStableSystemDefault()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    WallpaperCatalog catalog(WallpaperResolver(factory), temp.filePath(QStringLiteral("user")));

    catalog.refresh();
    const auto entry = catalog.resolve(QStringLiteral("astrea://wallpaper/default"));

    QVERIFY(entry.has_value());
    QCOMPARE(entry->origin(), WallpaperOrigin::System);
    QCOMPARE(entry->logicalId(), QStringLiteral("astrea://wallpaper/default"));
    QVERIFY(!entry->displayName().isEmpty());
}

void WallpaperCatalogTest::importsUserImageWithStableId()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto source = writeImage(temp.filePath(QStringLiteral("snow & café.png")));
    const auto userRoot = temp.filePath(QStringLiteral("user"));
    WallpaperCatalog catalog(WallpaperResolver(factory), userRoot);

    QString error;
    const auto imported = catalog.importWallpaper(source, &error);

    QVERIFY2(imported.has_value(), qPrintable(error));
    QCOMPARE(imported->origin(), WallpaperOrigin::User);
    QVERIFY(imported->logicalId().startsWith(QStringLiteral("astrea://wallpaper/user/")));
    QVERIFY(QFileInfo::exists(imported->source()));
    QVERIFY(!imported->source().contains(QStringLiteral("snow & café")));
    QCOMPARE(catalog.resolve(imported->logicalId()), imported);
}

void WallpaperCatalogTest::reusesDuplicateContent()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto source = writeImage(temp.filePath(QStringLiteral("same.png")));
    WallpaperCatalog catalog(WallpaperResolver(factory), temp.filePath(QStringLiteral("user")));

    QString firstError;
    const auto first = catalog.importWallpaper(source, &firstError);
    QString secondError;
    const auto second = catalog.importWallpaper(source, &secondError);

    QVERIFY2(first.has_value(), qPrintable(firstError));
    QVERIFY2(second.has_value(), qPrintable(secondError));
    QCOMPARE(first->logicalId(), second->logicalId());
    QCOMPARE(catalog.list().size(), 2);
}

void WallpaperCatalogTest::persistsDisplayNameAcrossRefreshAndReconstruction()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto source = writeImage(temp.filePath(QStringLiteral("snow & cafe.png")));
    const auto userRoot = temp.filePath(QStringLiteral("user"));
    WallpaperCatalog catalog(WallpaperResolver(factory), userRoot);

    QString error;
    const auto imported = catalog.importWallpaper(source, QStringLiteral("Snow Café"), &error);
    QVERIFY2(imported.has_value(), qPrintable(error));
    const auto logicalId = imported->logicalId();

    catalog.refresh();
    const auto refreshed = catalog.resolve(logicalId);
    QVERIFY(refreshed.has_value());
    QCOMPARE(refreshed->displayName(), QStringLiteral("Snow Café"));

    WallpaperCatalog reconstructed(WallpaperResolver(factory), userRoot);
    const auto restored = reconstructed.resolve(logicalId);
    QVERIFY(restored.has_value());
    QCOMPARE(restored->logicalId(), logicalId);
    QCOMPARE(restored->source(), refreshed->source());
    QCOMPARE(restored->displayName(), QStringLiteral("Snow Café"));
}

void WallpaperCatalogTest::updatesDuplicateDisplayNameWithoutSecondImage()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto source = writeImage(temp.filePath(QStringLiteral("same.png")));
    const auto userRoot = temp.filePath(QStringLiteral("user"));
    WallpaperCatalog catalog(WallpaperResolver(factory), userRoot);

    QString firstError;
    const auto first = catalog.importWallpaper(source, QStringLiteral("First name"), &firstError);
    QVERIFY2(first.has_value(), qPrintable(firstError));
    QString secondError;
    const auto second = catalog.importWallpaper(source, QStringLiteral("Second name"), &secondError);
    QVERIFY2(second.has_value(), qPrintable(secondError));
    QCOMPARE(first->logicalId(), second->logicalId());
    QCOMPARE(second->displayName(), QStringLiteral("Second name"));
    QCOMPARE(QDir(userRoot).entryList(QStringList{QStringLiteral("*.png")}, QDir::Files).size(), 1);

    QString preserveError;
    const auto preserved = catalog.importWallpaper(source, &preserveError);
    QVERIFY2(preserved.has_value(), qPrintable(preserveError));
    QCOMPARE(preserved->displayName(), QStringLiteral("Second name"));
}

void WallpaperCatalogTest::ignoresMalformedMetadataWithoutExposingDigest()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto source = writeImage(temp.filePath(QStringLiteral("legacy.png")));
    const auto userRoot = temp.filePath(QStringLiteral("user"));
    WallpaperCatalog catalog(WallpaperResolver(factory), userRoot);

    QString error;
    const auto imported = catalog.importWallpaper(source, QStringLiteral("Legacy name"), &error);
    QVERIFY2(imported.has_value(), qPrintable(error));
    const auto metadataFiles = QDir(userRoot).entryList(QStringList{QStringLiteral("*.json")},
                                                        QDir::Files);
    QCOMPARE(metadataFiles.size(), 1);
    QFile metadata(QDir(userRoot).filePath(metadataFiles.constFirst()));
    QVERIFY(metadata.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(metadata.write("not json") > 0);
    metadata.close();

    catalog.refresh();
    const auto restored = catalog.resolve(imported->logicalId());
    QVERIFY(restored.has_value());
    QCOMPARE(restored->displayName(), QStringLiteral("Wallpaper"));
    QVERIFY(!restored->displayName().contains(imported->logicalId().section(QLatin1Char('/'), -1)));
}

void WallpaperCatalogTest::rejectsFailedMetadataPublicationWithoutCatalogEntry()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto source = writeImage(temp.filePath(QStringLiteral("metadata-failure.png")));
    const auto userRoot = temp.filePath(QStringLiteral("user"));
    QVERIFY(QDir().mkpath(userRoot));

    QFile sourceFile(source);
    QVERIFY(sourceFile.open(QIODevice::ReadOnly));
    const auto digest = QString::fromLatin1(
        QCryptographicHash::hash(sourceFile.readAll(), QCryptographicHash::Sha256).toHex());
    sourceFile.close();
    QVERIFY(QDir().mkpath(QDir(userRoot).filePath(digest + QStringLiteral(".json"))));

    WallpaperCatalog catalog(WallpaperResolver(factory), userRoot);
    QString error;
    QVERIFY(!catalog.importWallpaper(source, QStringLiteral("Should not publish"), &error).has_value());
    QVERIFY(!error.isEmpty());
    QCOMPARE(catalog.list().size(), 1);
    QCOMPARE(QDir(userRoot).entryList(QStringList{QStringLiteral("*.png")}, QDir::Files).size(), 0);
}

void WallpaperCatalogTest::rejectsDirectoryAndUnsupportedImage()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto userRoot = temp.filePath(QStringLiteral("user"));
    WallpaperCatalog catalog(WallpaperResolver(factory), userRoot);

    QString directoryError;
    QVERIFY(!catalog.importWallpaper(temp.path(), &directoryError).has_value());
    QVERIFY(!directoryError.isEmpty());

    const auto textPath = temp.filePath(QStringLiteral("not-image.txt"));
    QFile text(textPath);
    QVERIFY(text.open(QIODevice::WriteOnly));
    QVERIFY(text.write("not an image") > 0);
    text.close();

    QString imageError;
    QVERIFY(!catalog.importWallpaper(textPath, &imageError).has_value());
    QVERIFY(!imageError.isEmpty());
    QCOMPARE(catalog.list().size(), 1);

    const auto namedSource = writeImage(temp.filePath(QStringLiteral("named.png")));
    QString longNameError;
    QVERIFY(!catalog.importWallpaper(namedSource, QString(129, QLatin1Char('x')), &longNameError).has_value());
    QVERIFY(!longNameError.isEmpty());
    QString controlNameError;
    QVERIFY(!catalog.importWallpaper(namedSource,
                                     QStringLiteral("bad\nname"),
                                     &controlNameError)
                 .has_value());
    QVERIFY(!controlNameError.isEmpty());
}

QTEST_MAIN(WallpaperCatalogTest)
#include "WallpaperCatalogTest.moc"
