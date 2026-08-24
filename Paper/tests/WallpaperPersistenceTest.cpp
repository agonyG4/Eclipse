#include "core/WallpaperPersistence.hpp"

#include <QTemporaryDir>
#include <QTest>

using namespace Paper;

class WallpaperPersistenceTest final : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsConfiguredSourceWithUnicodeAndSpaces();
    void clearRemovesConfiguredOverride();
    void missingFileLoadsEmptyState();
    void migratesValidLegacySymlinkOnce();
    void ignoresStaleLegacySymlink();
    void roundTripsStableSelectionWithoutSourcePath();
};

void WallpaperPersistenceTest::roundTripsConfiguredSourceWithUnicodeAndSpaces()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    XdgWallpaperPersistence persistence(temp.filePath(QStringLiteral("paper.ini")));
    const auto descriptor = WallpaperDescriptor::externalFile(
        QStringLiteral("/home/user/Images/snow & café.png"), WallpaperFit::Contain);

    QString error;
    QVERIFY2(persistence.save(descriptor, &error), qPrintable(error));

    const auto loaded = persistence.load(&error);
    QVERIFY2(loaded.has_value(), qPrintable(error));
    QCOMPARE(*loaded, descriptor);
}

void WallpaperPersistenceTest::clearRemovesConfiguredOverride()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    XdgWallpaperPersistence persistence(temp.filePath(QStringLiteral("paper.ini")));
    const auto descriptor = WallpaperDescriptor::externalFile(
        QStringLiteral("/tmp/wallpaper.png"), WallpaperFit::Cover);

    QString error;
    QVERIFY(persistence.save(descriptor, &error));
    QVERIFY2(persistence.clear(&error), qPrintable(error));
    QVERIFY(!persistence.load(&error).has_value());
}

void WallpaperPersistenceTest::missingFileLoadsEmptyState()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    XdgWallpaperPersistence persistence(temp.filePath(QStringLiteral("not-created.ini")));

    QString error;
    QVERIFY(!persistence.load(&error).has_value());
    QVERIFY(error.isEmpty());
}

void WallpaperPersistenceTest::migratesValidLegacySymlinkOnce()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto source = temp.filePath(QStringLiteral("legacy-source.png"));
    QFile sourceFile(source);
    QVERIFY(sourceFile.open(QIODevice::WriteOnly));
    QVERIFY(sourceFile.write("placeholder") > 0);
    sourceFile.close();
    const auto legacy = temp.filePath(QStringLiteral("legacy/wallpaper.jpg"));
    QVERIFY(QDir().mkpath(QFileInfo(legacy).absolutePath()));
    QVERIFY(QFile::link(source, legacy));

    XdgWallpaperPersistence persistence(temp.filePath(QStringLiteral("paper.ini")), legacy);
    const auto migrated = persistence.migrateLegacy();
    QVERIFY(migrated.has_value());
    QCOMPARE(migrated->source(), QFileInfo(source).canonicalFilePath());
    QCOMPARE(migrated->logicalId(), QStringLiteral("legacy:astreaos-paper"));

    QString error;
    QVERIFY(persistence.save(*migrated, &error));
    QVERIFY(!persistence.migrateLegacy().has_value());
}

void WallpaperPersistenceTest::ignoresStaleLegacySymlink()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto legacy = temp.filePath(QStringLiteral("legacy/wallpaper.jpg"));
    QVERIFY(QDir().mkpath(QFileInfo(legacy).absolutePath()));
    QVERIFY(QFile::link(temp.filePath(QStringLiteral("gone.png")), legacy));

    XdgWallpaperPersistence persistence(temp.filePath(QStringLiteral("paper.ini")), legacy);
    QVERIFY(!persistence.migrateLegacy().has_value());
}

void WallpaperPersistenceTest::roundTripsStableSelectionWithoutSourcePath()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    XdgWallpaperPersistence persistence(temp.filePath(QStringLiteral("paper.ini")));
    const WallpaperSelection selection{QStringLiteral("astrea://wallpaper/user/abc123"),
                                       WallpaperFit::Center};

    QString error;
    QVERIFY2(persistence.saveSelection(selection, &error), qPrintable(error));
    const auto loaded = persistence.loadSelection(&error);

    QVERIFY2(loaded.has_value(), qPrintable(error));
    QCOMPARE(loaded->wallpaperId, selection.wallpaperId);
    QCOMPARE(loaded->fit, selection.fit);
    QVERIFY(!persistence.load(&error).has_value());
}

QTEST_MAIN(WallpaperPersistenceTest)
#include "WallpaperPersistenceTest.moc"
