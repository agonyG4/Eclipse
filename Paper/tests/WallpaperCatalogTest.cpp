#include "core/WallpaperCatalog.hpp"

#include "core/WallpaperDescriptor.hpp"
#include "core/WallpaperResolver.hpp"

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
}

QTEST_MAIN(WallpaperCatalogTest)
#include "WallpaperCatalogTest.moc"
