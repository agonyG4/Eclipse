// End to end: the Rust tile renderer behind AstreaIconProvider's tile hook.

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "AstreaAppTiles.hpp"
#include "icons/AppTileHook.hpp"
#include "icons/AstreaIconProvider.hpp"

namespace {

bool writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

// A shaped logo: a red ring on transparency.
bool writeRingPng(const QString &path)
{
    QImage image(256, 256, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(220, 40, 40), 40));
    painter.drawEllipse(QRectF(48, 48, 160, 160));
    painter.end();
    QDir().mkpath(QFileInfo(path).absolutePath());
    return image.save(path);
}

QPixmap request(AstreaIconProvider &provider, const QString &name, const char *tile, int size,
                const QString &extra = {})
{
    QSize out;
    return provider.requestPixmap(
        QStringLiteral("%1?logicalSize=%2&dpr=1&tile=%3").arg(name).arg(size).arg(QLatin1String(tile))
            + extra,
        &out, {});
}

} // namespace

class AppTilesProviderTest : public QObject {
    Q_OBJECT

private:
    void useTestTheme(AstreaIconProvider &provider);

private slots:
    void initTestCase()
    {
        QVERIFY(m_home.isValid());
        qputenv("XDG_CACHE_HOME", (m_home.path() + QStringLiteral("/cache")).toUtf8());
        qputenv("XDG_CONFIG_HOME", (m_home.path() + QStringLiteral("/config")).toUtf8());
        AstreaTiles::installRustRenderer();

        const QString root = m_home.path() + QStringLiteral("/icons");
        // The Astrea theme draws "own-app" itself; "foreign-app" only exists in
        // hicolor, so it gets a tile.
        QVERIFY(writeFile(root + QStringLiteral("/Astrea-test/index.theme"),
                          "[Icon Theme]\nX-Astrea-AppTiles=true\nName=Astrea-test\nComment=fixture, with a comma as in real themes\n"
                          "Inherits=hicolor\nDirectories=apps/48\n\n"
                          "[apps/48]\nSize=48\nType=Fixed\nContext=Applications\n"));
        QImage own(48, 48, QImage::Format_ARGB32);
        own.fill(QColor(0, 200, 0));
        QVERIFY(QDir().mkpath(root + QStringLiteral("/Astrea-test/apps/48")));
        QVERIFY(own.save(root + QStringLiteral("/Astrea-test/apps/48/own-app.png")));
        QVERIFY(writeFile(root + QStringLiteral("/hicolor/index.theme"),
                          "[Icon Theme]\nName=Hicolor\nComment=fixture\nDirectories=256x256/apps\n\n"
                          "[256x256/apps]\nSize=256\nType=Fixed\nContext=Applications\n"));
        QVERIFY(writeRingPng(root + QStringLiteral("/hicolor/256x256/apps/foreign-app.png")));

        m_root = root;
    }


    void foreignArtGetsTileAtExactSize()
    {
        AstreaIconProvider provider;
        useTestTheme(provider);
        QVERIFY(provider.appTilesActive());
        QSignalSpy refreshed(&provider, &AstreaIconProvider::cacheInvalidated);

        // First request: the tile is being made, the plain art stands in.
        const QPixmap first = request(provider, QStringLiteral("foreign-app"), "dark", 40);
        QVERIFY(!first.isNull());
        QVERIFY(refreshed.wait(10000));

        for (const char *variant : {"dark", "light"}) {
            const QImage tile = request(provider, QStringLiteral("foreign-app"), variant, 40).toImage();
            QCOMPARE(tile.size(), QSize(40, 40));
            // corner: outside the squircle
            QCOMPARE(qAlpha(tile.pixel(0, 0)), 0);
            // the ring's hole shows the theme's tile
            const QColor hole = tile.pixelColor(20, 19);
            QCOMPARE(hole.alpha(), 255);
            if (qstrcmp(variant, "dark") == 0)
                QVERIFY2(hole.lightness() < 80, qPrintable(hole.name()));
            else
                QVERIFY2(hole.lightness() > 220, qPrintable(hole.name()));
        }

        // Made once, then served from the disk cache by a fresh provider.
        const QDir cache(m_home.path() + QStringLiteral("/cache/AstreaOS/app-tiles"));
        QCOMPARE(cache.entryList({QStringLiteral("*.png")}).size(), 2);
        AstreaIconProvider second;
        useTestTheme(second);
        QSignalSpy secondRefresh(&second, &AstreaIconProvider::cacheInvalidated);
        request(second, QStringLiteral("foreign-app"), "dark", 64);
        QVERIFY(secondRefresh.wait(10000));
        QCOMPARE(request(second, QStringLiteral("foreign-app"), "dark", 64).size(), QSize(64, 64));
    }

    void themeArtIsLeftAlone()
    {
        AstreaIconProvider provider;
        useTestTheme(provider);
        const QImage own = request(provider, QStringLiteral("own-app"), "dark", 48).toImage();
        QCOMPARE(own.pixelColor(24, 24), QColor(0, 200, 0));
        QCOMPARE(own.pixelColor(0, 0), QColor(0, 200, 0));
    }

    void stylesRestyleThemeArtAndTiles()
    {
        AstreaIconProvider provider;
        useTestTheme(provider);
        QVERIFY(provider.appStylesAvailable());
        QSignalSpy refreshed(&provider, &AstreaIconProvider::cacheInvalidated);

        // The theme's own green tile turns dark in the Dark style.
        request(provider, QStringLiteral("own-app"), "dark", 64, QStringLiteral("&style=dark"));
        QTRY_VERIFY_WITH_TIMEOUT(refreshed.count() > 0, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(
            request(provider, QStringLiteral("own-app"), "dark", 64, QStringLiteral("&style=dark"))
                .toImage().pixelColor(32, 48).lightness() < 70, 10000);

        // A tile in the Clear style is glass: see-through, not opaque.
        refreshed.clear();
        request(provider, QStringLiteral("foreign-app"), "dark", 64, QStringLiteral("&style=clear"));
        QTRY_VERIFY_WITH_TIMEOUT(refreshed.count() > 0, 10000);
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            const int a = request(provider, QStringLiteral("foreign-app"), "dark", 64,
                                  QStringLiteral("&style=clear")).toImage().pixelColor(12, 48).alpha();
            return a > 40 && a < 220;
        }(), 10000);

        // Tinted takes the tint.
        refreshed.clear();
        const QString tinted = QStringLiteral("&style=tinted&tint=0a84ff");
        request(provider, QStringLiteral("own-app"), "light", 64, tinted);
        QTRY_VERIFY_WITH_TIMEOUT(refreshed.count() > 0, 10000);
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            const QColor c = request(provider, QStringLiteral("own-app"), "light", 64, tinted)
                                 .toImage().pixelColor(32, 40);
            return c.blue() > c.red() + 80;
        }(), 10000);
    }

    void untiledRequestsAreUnchanged()
    {
        AstreaIconProvider provider;
        useTestTheme(provider);
        QSize out;
        const QImage plain = provider.requestPixmap(
            QStringLiteral("foreign-app?logicalSize=40&dpr=1"), &out, {}).toImage();
        QVERIFY(!plain.isNull());
        QCOMPARE(qAlpha(plain.pixel(20, 20)), 0);   // the ring's hole stays see-through
    }

private:
    QTemporaryDir m_home;
    QString m_root;
};

// The provider applies the user's configured theme when it is made; the
// test theme goes in after, as AstreaIconProviderTest does it.
void AppTilesProviderTest::useTestTheme(AstreaIconProvider &provider)
{
    QIcon::setThemeSearchPaths({m_root});
    QIcon::setFallbackSearchPaths({m_root});
    QIcon::setThemeName(QStringLiteral("Astrea-test"));
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));
    provider.clearCache();
}

QTEST_MAIN(AppTilesProviderTest)
#include "AppTilesProviderTest.moc"
