#include <QColor>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <cmath>
#include <limits>

#include "icons/AstreaIconProvider.hpp"
#include "icons/AstreaIconTheme.hpp"
#include "icons/IconRenderRequest.hpp"

namespace {

class ScopedIconState final {
public:
    ScopedIconState()
        : m_themeName(QIcon::themeName()),
          m_fallbackThemeName(QIcon::fallbackThemeName()),
          m_themeSearchPaths(QIcon::themeSearchPaths()),
          m_fallbackSearchPaths(QIcon::fallbackSearchPaths())
    {
    }

    ~ScopedIconState()
    {
        QIcon::setThemeName(m_themeName);
        QIcon::setFallbackThemeName(m_fallbackThemeName);
        QIcon::setThemeSearchPaths(m_themeSearchPaths);
        QIcon::setFallbackSearchPaths(m_fallbackSearchPaths);
    }

private:
    QString m_themeName;
    QString m_fallbackThemeName;
    QStringList m_themeSearchPaths;
    QStringList m_fallbackSearchPaths;
};

bool writeThemeIndex(const QString &themeRoot)
{
    QFile index(themeRoot + QStringLiteral("/index.theme"));
    if (!index.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    const QByteArray content =
        "[Icon Theme]\n"
        "Name=Resolution Test\n"
        "Directories=48x48/apps,96x96/apps,128x128/apps,scalable/apps\n"
        "Inherits=hicolor\n"
        "\n"
        "[48x48/apps]\nSize=48\nContext=Applications\nType=Fixed\n\n"
        "[96x96/apps]\nSize=96\nContext=Applications\nType=Fixed\n\n"
        "[128x128/apps]\nSize=128\nContext=Applications\nType=Fixed\n\n"
        "[scalable/apps]\nSize=48\nMinSize=1\nMaxSize=256\nContext=Applications\nType=Scalable\n";
    return index.write(content) == content.size();
}

bool writeRaster(const QString &path, const QSize &size, const QColor &color)
{
    QImage image(size, QImage::Format_ARGB32);
    image.fill(color);
    QPainter painter(&image);
    painter.setPen(Qt::white);
    painter.drawLine(0, 0, size.width() - 1, size.height() - 1);
    painter.end();
    return image.save(path);
}

bool writeSvg(const QString &path)
{
    QFile svg(path);
    if (!svg.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    const QByteArray content =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"32\" height=\"32\">"
        "<rect width=\"32\" height=\"32\" fill=\"#00aa55\"/></svg>";
    return svg.write(content) == content.size();
}

QString prepareTheme(QTemporaryDir &temporary, const QString &themeName)
{
    const QString themeRoot = temporary.path() + QStringLiteral("/") + themeName;
    if (!QDir().mkpath(themeRoot + QStringLiteral("/48x48/apps"))
        || !QDir().mkpath(themeRoot + QStringLiteral("/96x96/apps"))
        || !QDir().mkpath(themeRoot + QStringLiteral("/128x128/apps"))
        || !QDir().mkpath(themeRoot + QStringLiteral("/scalable/apps"))
        || !writeThemeIndex(themeRoot)) {
        return {};
    }
    return themeRoot;
}

void selectTheme(AstreaIconProvider &provider, const QString &root, const QString &theme)
{
    QIcon::setThemeSearchPaths({root});
    QIcon::setFallbackSearchPaths({root});
    QIcon::setThemeName(theme);
    provider.clearCache();
}

} // namespace

class AstreaIconProviderTest final : public QObject {
    Q_OBJECT

private slots:
    void normalizesResolutionRequest();
    void rejectsInvalidResolutionRequest();
    void providerUsesThemedPhysicalRepresentation();
    void providerDoesNotUpscaleSmallerRaster();
    void providerSeparatesLogicalAndDprCacheKeys();
    void providerSupportsDirectAndScalableFiles();
    void providerHandlesMissingAndBoundedInvalidRequests();
    void providerInvalidatesPositiveAndNegativeCachesOnThemeChange();
    void searchPathsPreservePriorityAndDeduplicate();
    void applyMergesExistingQtSearchPaths();
};

void AstreaIconProviderTest::normalizesResolutionRequest()
{
    const auto request = IconRenderRequest::fromValues(48, 2);
    QVERIFY(request.has_value());
    QCOMPARE(request->logicalExtent, 48);
    QCOMPARE(request->devicePixelRatio, 2.0);
    QCOMPARE(request->physicalExtent, 96);

    const auto fractional = IconRenderRequest::fromValues(48 * 1.6, 1.5);
    QVERIFY(fractional.has_value());
    QCOMPARE(fractional->logicalExtent, 77);
    QCOMPARE(fractional->physicalExtent, 116);

    const auto bounded = IconRenderRequest::fromValues(10000, 10000);
    QVERIFY(bounded.has_value());
    QVERIFY(bounded->logicalExtent <= IconRenderRequest::kMaxLogicalExtent);
    QVERIFY(bounded->physicalExtent <= IconRenderRequest::kMaxPhysicalExtent);
}

void AstreaIconProviderTest::rejectsInvalidResolutionRequest()
{
    QVERIFY(!IconRenderRequest::fromValues(0, 1).has_value());
    QVERIFY(!IconRenderRequest::fromValues(-1, 1).has_value());
    QVERIFY(!IconRenderRequest::fromValues(48, 0).has_value());
    QVERIFY(!IconRenderRequest::fromValues(48, -1).has_value());
    QVERIFY(!IconRenderRequest::fromValues(std::numeric_limits<qreal>::quiet_NaN(), 1).has_value());
    QVERIFY(!IconRenderRequest::fromValues(48, std::numeric_limits<qreal>::infinity()).has_value());
}

void AstreaIconProviderTest::providerUsesThemedPhysicalRepresentation()
{
    ScopedIconState state;
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString themeRoot = prepareTheme(temporary, QStringLiteral("resolution-test"));
    QVERIFY(!themeRoot.isEmpty());
    QVERIFY(writeRaster(themeRoot + QStringLiteral("/48x48/apps/astrea-mark.png"),
                        QSize(48, 48), QColor("#aa0000")));
    QVERIFY(writeRaster(themeRoot + QStringLiteral("/96x96/apps/astrea-mark.png"),
                        QSize(96, 96), QColor("#00aa00")));
    QVERIFY(writeRaster(themeRoot + QStringLiteral("/128x128/apps/astrea-mark.png"),
                        QSize(128, 128), QColor("#0000aa")));

    AstreaIconProvider provider;
    selectTheme(provider, temporary.path(), QStringLiteral("resolution-test"));
    QSize returnedSize;
    const QPixmap pixmap = provider.requestPixmap(
        QStringLiteral("astrea-mark?logicalSize=48&dpr=2&pixelSize=96"),
        &returnedSize, {});

    QCOMPARE(returnedSize, QSize(96, 96));
    QCOMPARE(pixmap.size(), QSize(96, 96));
    QCOMPARE(pixmap.toImage().pixelColor(10, 20), QColor("#00aa00"));

    QSize extensionSize;
    const QPixmap extensionPixmap = provider.requestPixmap(
        QStringLiteral("astrea-mark.png?logicalSize=48&dpr=2"), &extensionSize, {});
    QCOMPARE(extensionSize, QSize(96, 96));
    QCOMPARE(extensionPixmap.toImage().pixelColor(10, 20), QColor("#00aa00"));
}

void AstreaIconProviderTest::providerDoesNotUpscaleSmallerRaster()
{
    ScopedIconState state;
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString themeRoot = prepareTheme(temporary, QStringLiteral("small-only"));
    QVERIFY(!themeRoot.isEmpty());
    QVERIFY(writeRaster(themeRoot + QStringLiteral("/48x48/apps/small-mark.png"),
                        QSize(48, 48), QColor("#aa00aa")));

    AstreaIconProvider provider;
    selectTheme(provider, temporary.path(), QStringLiteral("small-only"));
    QSize returnedSize;
    const QPixmap pixmap = provider.requestPixmap(
        QStringLiteral("small-mark?logicalSize=96&dpr=1&pixelSize=96"),
        &returnedSize, {});

    QCOMPARE(returnedSize, QSize(48, 48));
    QCOMPARE(pixmap.size(), QSize(48, 48));
    QCOMPARE(pixmap.toImage().pixelColor(10, 20), QColor("#aa00aa"));
}

void AstreaIconProviderTest::providerSeparatesLogicalAndDprCacheKeys()
{
    ScopedIconState state;
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString themeRoot = prepareTheme(temporary, QStringLiteral("cache-key-test"));
    QVERIFY(!themeRoot.isEmpty());
    QVERIFY(writeRaster(themeRoot + QStringLiteral("/48x48/apps/cache-mark.png"),
                        QSize(48, 48), QColor("#aa0000")));
    QVERIFY(writeRaster(themeRoot + QStringLiteral("/96x96/apps/cache-mark.png"),
                        QSize(96, 96), QColor("#00aa00")));

    AstreaIconProvider provider;
    selectTheme(provider, temporary.path(), QStringLiteral("cache-key-test"));
    QSize oneXSize;
    QSize twoXSize;
    const QPixmap oneX = provider.requestPixmap(
        QStringLiteral("cache-mark?logicalSize=48&dpr=1&pixelSize=48"), &oneXSize, {});
    const QPixmap twoX = provider.requestPixmap(
        QStringLiteral("cache-mark?logicalSize=48&dpr=2&pixelSize=96"), &twoXSize, {});

    QCOMPARE(oneXSize, QSize(48, 48));
    QCOMPARE(twoXSize, QSize(96, 96));
    QCOMPARE(oneX.toImage().pixelColor(10, 20), QColor("#aa0000"));
    QCOMPARE(twoX.toImage().pixelColor(10, 20), QColor("#00aa00"));
}

void AstreaIconProviderTest::providerSupportsDirectAndScalableFiles()
{
    ScopedIconState state;
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString rasterPath = temporary.path() + QStringLiteral("/direct.png");
    const QString svgPath = temporary.path() + QStringLiteral("/scalable.svg");
    QVERIFY(writeRaster(rasterPath, QSize(64, 64), QColor("#ffaa00")));
    QVERIFY(writeSvg(svgPath));

    AstreaIconProvider provider;
    QSize directSize;
    const QPixmap direct = provider.requestPixmap(
        QUrl::fromLocalFile(rasterPath).toString()
            + QStringLiteral("?logicalSize=32&dpr=2&pixelSize=64"),
        &directSize, {});
    QCOMPARE(directSize, QSize(64, 64));
    QCOMPARE(direct.toImage().pixelColor(10, 20), QColor("#ffaa00"));

    QSize svgSize;
    const QPixmap svg = provider.requestPixmap(
        QUrl::fromLocalFile(svgPath).toString()
            + QStringLiteral("?logicalSize=40&dpr=2&pixelSize=80"),
        &svgSize, {});
    QCOMPARE(svgSize, QSize(80, 80));
    QVERIFY(!svg.isNull());
}

void AstreaIconProviderTest::providerHandlesMissingAndBoundedInvalidRequests()
{
    ScopedIconState state;
    AstreaIconProvider provider;

    QSize emptySize;
    QVERIFY(provider.requestPixmap(QStringLiteral("?logicalSize=0&dpr=0"),
                                   &emptySize, {}).isNull());
    QCOMPARE(emptySize, QSize(80, 80));

    QSize invalidSize;
    QVERIFY(provider.requestPixmap(
                 QStringLiteral("missing?logicalSize=nan&dpr=inf"), &invalidSize, {})
                .isNull());
    QCOMPARE(invalidSize, QSize(80, 80));

    QSize hugeSize;
    QVERIFY(provider.requestPixmap(
                 QStringLiteral("missing?logicalSize=999999999&dpr=999999999"),
                 &hugeSize, {})
                .isNull());
    QCOMPARE(hugeSize, QSize(1024, 1024));
}

void AstreaIconProviderTest::providerInvalidatesPositiveAndNegativeCachesOnThemeChange()
{
    ScopedIconState state;
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    const QString firstTheme = prepareTheme(first, QStringLiteral("first-theme"));
    const QString secondTheme = prepareTheme(second, QStringLiteral("second-theme"));
    QVERIFY(!firstTheme.isEmpty() && !secondTheme.isEmpty());
    QVERIFY(writeRaster(firstTheme + QStringLiteral("/48x48/apps/switch-mark.png"),
                        QSize(48, 48), QColor("#aa0000")));
    QVERIFY(writeRaster(secondTheme + QStringLiteral("/48x48/apps/switch-mark.png"),
                        QSize(48, 48), QColor("#0000aa")));

    AstreaIconProvider provider;
    selectTheme(provider, first.path(), QStringLiteral("first-theme"));
    QSize firstSize;
    const QPixmap firstPixmap = provider.requestPixmap(
        QStringLiteral("switch-mark?logicalSize=48&dpr=1"), &firstSize, {});
    QCOMPARE(firstSize, QSize(48, 48));
    QCOMPARE(firstPixmap.toImage().pixelColor(10, 20), QColor("#aa0000"));

    QSize missingSize;
    QVERIFY(provider.requestPixmap(
                 QStringLiteral("appears-later?logicalSize=48&dpr=1"), &missingSize, {})
                .isNull());

    selectTheme(provider, second.path(), QStringLiteral("second-theme"));
    QSize secondSize;
    const QPixmap secondPixmap = provider.requestPixmap(
        QStringLiteral("switch-mark?logicalSize=48&dpr=1"), &secondSize, {});
    QCOMPARE(secondSize, QSize(48, 48));
    QCOMPARE(secondPixmap.toImage().pixelColor(10, 20), QColor("#0000aa"));

    QVERIFY(writeRaster(secondTheme + QStringLiteral("/48x48/apps/appears-later.png"),
                        QSize(48, 48), QColor("#00aa00")));
    provider.clearCache();
    QSize appearedSize;
    const QPixmap appeared = provider.requestPixmap(
        QStringLiteral("appears-later?logicalSize=48&dpr=1"), &appearedSize, {});
    QCOMPARE(appearedSize, QSize(48, 48));
    QCOMPARE(appeared.toImage().pixelColor(10, 20), QColor("#00aa00"));
}

void AstreaIconProviderTest::searchPathsPreservePriorityAndDeduplicate()
{
    QTemporaryDir dataHome;
    QTemporaryDir dataDir;
    QTemporaryDir fakeHome;
    QVERIFY(dataHome.isValid());
    QVERIFY(dataDir.isValid());
    QVERIFY(fakeHome.isValid());
    QVERIFY(QDir().mkpath(dataHome.path() + QStringLiteral("/icons")));
    QVERIFY(QDir().mkpath(dataDir.path() + QStringLiteral("/icons")));
    QVERIFY(QDir().mkpath(fakeHome.path() + QStringLiteral("/.icons")));
    QVERIFY(QDir().mkpath(fakeHome.path()
                          + QStringLiteral("/.local/share/flatpak/exports/share/icons")));

    const QStringList paths = AstreaIconTheme::searchPathsFor(
        {dataHome.path(), dataDir.path(), dataHome.path()}, fakeHome.path());
    QCOMPARE(paths.count(dataHome.path() + QStringLiteral("/icons")), 1);
    QVERIFY(paths.indexOf(dataHome.path() + QStringLiteral("/icons"))
            < paths.indexOf(dataDir.path() + QStringLiteral("/icons")));
    QVERIFY(paths.contains(fakeHome.path() + QStringLiteral("/.icons")));
    QVERIFY(paths.contains(fakeHome.path()
                           + QStringLiteral("/.local/share/flatpak/exports/share/icons")));
}

void AstreaIconProviderTest::applyMergesExistingQtSearchPaths()
{
    ScopedIconState state;
    QTemporaryDir existing;
    QVERIFY(existing.isValid());
    QIcon::setThemeSearchPaths({existing.path()});
    QIcon::setFallbackSearchPaths({existing.path()});

    AstreaIconTheme::apply();

    QVERIFY(QIcon::themeSearchPaths().contains(existing.path()));
    QVERIFY(QIcon::fallbackSearchPaths().contains(existing.path()));
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    AstreaIconProviderTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "AstreaIconProviderTest.moc"
