#include <QColor>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <atomic>
#include <cmath>
#include <limits>
#include <thread>

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

class ScopedEnvironment final {
public:
    void set(const char *name, const QByteArray &value)
    {
        const QString key = QString::fromLatin1(name);
        if (!m_previous.contains(key))
            m_previous.insert(key, {qEnvironmentVariableIsSet(name), qgetenv(name)});
        qputenv(name, value);
    }

    ~ScopedEnvironment()
    {
        for (auto it = m_previous.constBegin(); it != m_previous.constEnd(); ++it) {
            const QByteArray name = it.key().toLatin1();
            if (it.value().first)
                qputenv(name.constData(), it.value().second);
            else
                qunsetenv(name.constData());
        }
    }

private:
    QHash<QString, QPair<bool, QByteArray>> m_previous;
};

bool writeIndexFile(const QString &themeRoot, const QByteArray &content)
{
    QFile index(themeRoot + QStringLiteral("/index.theme"));
    if (!index.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    return index.write(content) == content.size();
}

bool writeThemeIndex(const QString &themeRoot)
{
    return writeIndexFile(themeRoot,
        QByteArray(
        "[Icon Theme]\n"
        "Name=Resolution Test\n"
        "Directories=48x48/apps,96x96/apps,128x128/apps,scalable/apps\n"
        "Inherits=hicolor\n"
        "\n"
        "[48x48/apps]\nSize=48\nContext=Applications\nType=Fixed\n\n"
        "[96x96/apps]\nSize=96\nContext=Applications\nType=Fixed\n\n"
        "[128x128/apps]\nSize=128\nContext=Applications\nType=Fixed\n\n"
        "[scalable/apps]\nSize=48\nMinSize=1\nMaxSize=256\nContext=Applications\nType=Scalable\n"));
}

bool writeScaleAwareThemeIndex(const QString &themeRoot)
{
    QFile index(themeRoot + QStringLiteral("/index.theme"));
    if (!index.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    const QByteArray content =
        "[Icon Theme]\n"
        "Name=HiDPI Test\n"
        "Directories=48x48/apps\n"
        "ScaledDirectories=48x48@2/apps\n"
        "\n"
        "[48x48/apps]\nSize=48\nScale=1\nType=Fixed\nContext=Applications\n\n"
        "[48x48@2/apps]\nSize=48\nScale=2\nType=Fixed\nContext=Applications\n";
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

bool writeHighFrequencySvg(const QString &path)
{
    QFile svg(path);
    if (!svg.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    const QByteArray content =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"128\" height=\"128\">"
        "<rect width=\"128\" height=\"128\" fill=\"#101820\"/>"
        "<path d=\"M0 0L128 128M-32 0L128 160M0 32L96 128M32 0L128 96M64 0L128 64\" "
        "stroke=\"#00d084\" stroke-width=\"3\"/>"
        "<circle cx=\"64\" cy=\"64\" r=\"24\" fill=\"none\" stroke=\"#ffd166\" stroke-width=\"4\"/>"
        "</svg>";
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
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));
    provider.clearCache();
}

} // namespace

class AstreaIconProviderTest final : public QObject {
    Q_OBJECT

private slots:
    void normalizesResolutionRequest();
    void rejectsInvalidResolutionRequest();
    void providerPreservesQtThemeRepresentation();
    void providerHonorsThemeScaleMetadata();
    void providerResolvesNamedScalableSvgAtPhysicalTarget();
    void providerFollowsQtThresholdSelection();
    void providerFallsBackThroughThemeInheritance();
    void providerPrefersCurrentThemeOverCloserInheritedSize();
    void providerFallsBackToHicolor();
    void providerDoesNotUpscaleSmallerRaster();
    void providerSeparatesLogicalAndDprCacheKeys();
    void providerSupportsDirectAndScalableFiles();
    void providerHandlesMissingAndBoundedInvalidRequests();
    void providerInvalidatesPositiveAndNegativeCachesOnThemeChange();
    void providerSurvivesConcurrentThemeInvalidation();
    void searchPathsPreservePriorityAndDeduplicate();
    void applyPreservesSplitThemePriority();
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

void AstreaIconProviderTest::providerPreservesQtThemeRepresentation()
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
    const QPixmap expected = QIcon::fromTheme(QStringLiteral("astrea-mark"))
                                 .pixmap(QSize(48, 48), 2.0, QIcon::Normal, QIcon::Off);
    QVERIFY(!expected.isNull());

    QSize returnedSize;
    const QPixmap pixmap = provider.requestPixmap(
        QStringLiteral("astrea-mark?logicalSize=48&dpr=2&pixelSize=96"),
        &returnedSize, {});

    QCOMPARE(returnedSize, expected.size());
    QCOMPARE(pixmap.size(), expected.size());
    QCOMPARE(pixmap.toImage().pixelColor(10, 20), expected.toImage().pixelColor(10, 20));

    QSize extensionSize;
    const QPixmap extensionPixmap = provider.requestPixmap(
        QStringLiteral("astrea-mark.png?logicalSize=48&dpr=2"), &extensionSize, {});
    QCOMPARE(extensionSize, expected.size());
    QCOMPARE(extensionPixmap.toImage().pixelColor(10, 20), expected.toImage().pixelColor(10, 20));

    QSize fractionalSize;
    const QPixmap fractional = provider.requestPixmap(
        QStringLiteral("astrea-mark?logicalSize=64&dpr=1.5"), &fractionalSize, {});
    const QPixmap fractionalExpected = QIcon::fromTheme(QStringLiteral("astrea-mark"))
                                           .pixmap(QSize(64, 64), 1.5,
                                                   QIcon::Normal, QIcon::Off);
    QVERIFY(!fractionalExpected.isNull());
    QCOMPARE(fractionalSize, fractionalExpected.size());
    QCOMPARE(fractional.toImage().pixelColor(10, 20),
             fractionalExpected.toImage().pixelColor(10, 20));
}

void AstreaIconProviderTest::providerHonorsThemeScaleMetadata()
{
    ScopedIconState state;
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString themeRoot = temporary.path() + QStringLiteral("/hidpi-test");
    QVERIFY(QDir().mkpath(themeRoot + QStringLiteral("/48x48/apps")));
    QVERIFY(QDir().mkpath(themeRoot + QStringLiteral("/48x48@2/apps")));
    QVERIFY(writeScaleAwareThemeIndex(themeRoot));
    QVERIFY(writeRaster(themeRoot + QStringLiteral("/48x48/apps/scale-test.png"),
                        QSize(48, 48), QColor("#aa0000")));
    QVERIFY(writeRaster(themeRoot + QStringLiteral("/48x48@2/apps/scale-test.png"),
                        QSize(96, 96), QColor("#00aa00")));

    AstreaIconProvider provider;
    selectTheme(provider, temporary.path(), QStringLiteral("hidpi-test"));

    QSize oneXSize;
    const QPixmap oneX = provider.requestPixmap(
        QStringLiteral("scale-test?logicalSize=48&dpr=1"), &oneXSize, {});
    QCOMPARE(oneXSize, QSize(48, 48));
    QCOMPARE(oneX.toImage().pixelColor(10, 20), QColor("#aa0000"));

    QSize twoXSize;
    const QPixmap twoX = provider.requestPixmap(
        QStringLiteral("scale-test?logicalSize=48&dpr=2"), &twoXSize, {});
    QCOMPARE(twoXSize, QSize(96, 96));
    QCOMPARE(twoX.devicePixelRatio(), 1.0);
    QCOMPARE(twoX.toImage().pixelColor(10, 20), QColor("#00aa00"));
}

void AstreaIconProviderTest::providerResolvesNamedScalableSvgAtPhysicalTarget()
{
    ScopedIconState state;
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString themeRoot = temporary.path() + QStringLiteral("/scalable-test");
    QVERIFY(QDir().mkpath(themeRoot + QStringLiteral("/scalable/apps")));
    QVERIFY(writeIndexFile(themeRoot,
                           QByteArray(
                               "[Icon Theme]\n"
                               "Name=Scalable Test\n"
                               "Directories=scalable/apps\n\n"
                               "[scalable/apps]\nSize=48\nMinSize=1\nMaxSize=256\n"
                               "Type=Scalable\nContext=Applications\n")));
    QVERIFY(writeHighFrequencySvg(themeRoot + QStringLiteral("/scalable/apps/vector-quality.svg")));

    AstreaIconProvider provider;
    selectTheme(provider, temporary.path(), QStringLiteral("scalable-test"));

    QSize oneXSize;
    const QPixmap oneX = provider.requestPixmap(
        QStringLiteral("vector-quality?logicalSize=96&dpr=1"), &oneXSize, {});
    QCOMPARE(oneXSize, QSize(96, 96));
    QVERIFY(!oneX.isNull());

    QSize twoXSize;
    const QPixmap twoX = provider.requestPixmap(
        QStringLiteral("vector-quality?logicalSize=48&dpr=2"), &twoXSize, {});
    QCOMPARE(twoXSize, QSize(96, 96));
    QVERIFY(!twoX.isNull());

    QSize largerTwoXSize;
    const QPixmap largerTwoX = provider.requestPixmap(
        QStringLiteral("vector-quality?logicalSize=96&dpr=2"), &largerTwoXSize, {});
    QCOMPARE(largerTwoXSize, QSize(192, 192));
    QVERIFY(!largerTwoX.isNull());
}

void AstreaIconProviderTest::providerFollowsQtThresholdSelection()
{
    ScopedIconState state;
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString themeRoot = temporary.path() + QStringLiteral("/threshold-test");
    QVERIFY(QDir().mkpath(themeRoot + QStringLiteral("/48x48/apps")));
    QVERIFY(QDir().mkpath(themeRoot + QStringLiteral("/64x64/apps")));
    QVERIFY(writeIndexFile(themeRoot,
                           QByteArray(
                               "[Icon Theme]\n"
                               "Name=Threshold Test\n"
                               "Directories=48x48/apps,64x64/apps\n\n"
                               "[48x48/apps]\nSize=48\nType=Threshold\nThreshold=4\n"
                               "Scale=1\nContext=Applications\n\n"
                               "[64x64/apps]\nSize=64\nType=Fixed\nScale=1\n"
                               "Context=Applications\n")));
    QVERIFY(writeRaster(themeRoot + QStringLiteral("/48x48/apps/threshold-test.png"),
                        QSize(48, 48), QColor("#aa0000")));
    QVERIFY(writeRaster(themeRoot + QStringLiteral("/64x64/apps/threshold-test.png"),
                        QSize(64, 64), QColor("#0000aa")));

    AstreaIconProvider provider;
    selectTheme(provider, temporary.path(), QStringLiteral("threshold-test"));
    const QIcon icon = QIcon::fromTheme(QStringLiteral("threshold-test"));
    QVERIFY(!icon.isNull());

    const auto verifyQtResult = [&](const QString &requestId, int logicalSize) {
        QSize providerSize;
        const QPixmap actual = provider.requestPixmap(requestId, &providerSize, {});
        QPixmap expected = icon.pixmap(QSize(logicalSize, logicalSize), 1.0,
                                       QIcon::Normal, QIcon::Off);
        expected.setDevicePixelRatio(1.0);
        QVERIFY(!actual.isNull());
        QVERIFY(!expected.isNull());
        QCOMPARE(providerSize, expected.size());
        QCOMPARE(actual.size(), expected.size());
        QCOMPARE(actual.toImage().pixelColor(10, 20), expected.toImage().pixelColor(10, 20));
    };

    verifyQtResult(QStringLiteral("threshold-test?logicalSize=50&dpr=1"), 50);
    verifyQtResult(QStringLiteral("threshold-test?logicalSize=58&dpr=1"), 58);
}

void AstreaIconProviderTest::providerFallsBackThroughThemeInheritance()
{
    ScopedIconState state;
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString childRoot = temporary.path() + QStringLiteral("/child-theme");
    const QString parentRoot = temporary.path() + QStringLiteral("/parent-theme");
    QVERIFY(QDir().mkpath(childRoot + QStringLiteral("/48x48/apps")));
    QVERIFY(QDir().mkpath(parentRoot + QStringLiteral("/48x48/apps")));
    QVERIFY(writeIndexFile(childRoot,
                           QByteArray("[Icon Theme]\nName=Child\n"
                                      "Directories=48x48/apps\nInherits=parent-theme\n\n"
                                      "[48x48/apps]\nSize=48\nType=Fixed\nContext=Applications\n")));
    QVERIFY(writeIndexFile(parentRoot,
                           QByteArray("[Icon Theme]\nName=Parent\n"
                                      "Directories=48x48/apps\n\n"
                                      "[48x48/apps]\nSize=48\nType=Fixed\nContext=Applications\n")));
    QVERIFY(writeRaster(parentRoot + QStringLiteral("/48x48/apps/inherited-test.png"),
                        QSize(48, 48), QColor("#00aa00")));

    AstreaIconProvider provider;
    selectTheme(provider, temporary.path(), QStringLiteral("child-theme"));
    QSize size;
    const QPixmap pixmap = provider.requestPixmap(
        QStringLiteral("inherited-test?logicalSize=48&dpr=1"), &size, {});
    QCOMPARE(size, QSize(48, 48));
    QCOMPARE(pixmap.toImage().pixelColor(10, 20), QColor("#00aa00"));
}

void AstreaIconProviderTest::providerPrefersCurrentThemeOverCloserInheritedSize()
{
    ScopedIconState state;
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString currentRoot = temporary.path() + QStringLiteral("/current-theme");
    const QString parentRoot = temporary.path() + QStringLiteral("/parent-theme");
    QVERIFY(QDir().mkpath(currentRoot + QStringLiteral("/64x64/apps")));
    QVERIFY(QDir().mkpath(parentRoot + QStringLiteral("/48x48/apps")));
    QVERIFY(writeIndexFile(currentRoot,
                           QByteArray("[Icon Theme]\nName=Current\n"
                                      "Directories=64x64/apps\nInherits=parent-theme\n\n"
                                      "[64x64/apps]\nSize=64\nType=Fixed\nContext=Applications\n")));
    QVERIFY(writeIndexFile(parentRoot,
                           QByteArray("[Icon Theme]\nName=Parent\n"
                                      "Directories=48x48/apps\n\n"
                                      "[48x48/apps]\nSize=48\nType=Fixed\nContext=Applications\n")));
    QVERIFY(writeRaster(currentRoot + QStringLiteral("/64x64/apps/current-wins.png"),
                        QSize(64, 64), QColor("#0000aa")));
    QVERIFY(writeRaster(parentRoot + QStringLiteral("/48x48/apps/current-wins.png"),
                        QSize(48, 48), QColor("#aa0000")));

    AstreaIconProvider provider;
    selectTheme(provider, temporary.path(), QStringLiteral("current-theme"));
    QSize size;
    const QPixmap pixmap = provider.requestPixmap(
        QStringLiteral("current-wins?logicalSize=48&dpr=1"), &size, {});
    QCOMPARE(pixmap.toImage().pixelColor(10, 20), QColor("#0000aa"));
}

void AstreaIconProviderTest::providerFallsBackToHicolor()
{
    ScopedIconState state;
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString currentRoot = temporary.path() + QStringLiteral("/current-theme");
    const QString hicolorRoot = temporary.path() + QStringLiteral("/hicolor");
    QVERIFY(QDir().mkpath(currentRoot + QStringLiteral("/48x48/apps")));
    QVERIFY(QDir().mkpath(hicolorRoot + QStringLiteral("/48x48/apps")));
    QVERIFY(writeIndexFile(currentRoot,
                           QByteArray("[Icon Theme]\nName=Current\n"
                                      "Directories=48x48/apps\n\n"
                                      "[48x48/apps]\nSize=48\nType=Fixed\nContext=Applications\n")));
    QVERIFY(writeIndexFile(hicolorRoot,
                           QByteArray("[Icon Theme]\nName=hicolor\n"
                                      "Directories=48x48/apps\n\n"
                                      "[48x48/apps]\nSize=48\nType=Fixed\nContext=Applications\n")));
    QVERIFY(writeRaster(hicolorRoot + QStringLiteral("/48x48/apps/fallback-test.png"),
                        QSize(48, 48), QColor("#00aaaa")));

    AstreaIconProvider provider;
    selectTheme(provider, temporary.path(), QStringLiteral("current-theme"));
    QSize size;
    const QPixmap pixmap = provider.requestPixmap(
        QStringLiteral("fallback-test?logicalSize=48&dpr=1"), &size, {});
    QCOMPARE(size, QSize(48, 48));
    QCOMPARE(pixmap.toImage().pixelColor(10, 20), QColor("#00aaaa"));
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

void AstreaIconProviderTest::providerSurvivesConcurrentThemeInvalidation()
{
    ScopedIconState state;
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString firstTheme = prepareTheme(temporary, QStringLiteral("stress-first"));
    const QString secondTheme = temporary.path() + QStringLiteral("/stress-second");
    QVERIFY(!firstTheme.isEmpty());
    QVERIFY(QDir().mkpath(secondTheme + QStringLiteral("/48x48/apps")));
    QVERIFY(QDir().mkpath(secondTheme + QStringLiteral("/96x96/apps")));
    QVERIFY(QDir().mkpath(secondTheme + QStringLiteral("/128x128/apps")));
    QVERIFY(QDir().mkpath(secondTheme + QStringLiteral("/scalable/apps")));
    QVERIFY(writeThemeIndex(secondTheme));
    QVERIFY(writeRaster(firstTheme + QStringLiteral("/48x48/apps/stress-test.png"),
                        QSize(48, 48), QColor("#aa0000")));
    QVERIFY(writeRaster(secondTheme + QStringLiteral("/48x48/apps/stress-test.png"),
                        QSize(48, 48), QColor("#0000aa")));

    AstreaIconProvider provider;
    selectTheme(provider, temporary.path(), QStringLiteral("stress-first"));
    std::atomic_bool failed = false;
    std::thread requester([&] {
        for (int i = 0; i < 128 && !failed.load(std::memory_order_relaxed); ++i) {
            QSize size;
            const QPixmap pixmap = provider.requestPixmap(
                QStringLiteral("stress-test?logicalSize=48&dpr=1"), &size, {});
            if (pixmap.isNull() || size != QSize(48, 48)) {
                failed.store(true, std::memory_order_relaxed);
                break;
            }
            const QColor pixel = pixmap.toImage().pixelColor(10, 20);
            if (pixel != QColor("#aa0000") && pixel != QColor("#0000aa"))
                failed.store(true, std::memory_order_relaxed);
        }
    });

    for (int i = 0; i < 32 && !failed.load(std::memory_order_relaxed); ++i) {
        const QString theme = i % 2 == 0 ? QStringLiteral("stress-second")
                                         : QStringLiteral("stress-first");
        {
            QMutexLocker lock(&AstreaIconTheme::qIconMutex());
            QIcon::setThemeName(theme);
            QIcon::setFallbackThemeName(QStringLiteral("hicolor"));
            QIcon::setThemeSearchPaths({temporary.path()});
            QIcon::setFallbackSearchPaths({temporary.path()});
        }
        provider.clearCache();
    }
    requester.join();
    QVERIFY(!failed.load(std::memory_order_relaxed));
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
    QVERIFY(paths.indexOf(fakeHome.path() + QStringLiteral("/.icons"))
            < paths.indexOf(dataHome.path() + QStringLiteral("/icons")));
    QVERIFY(paths.contains(fakeHome.path() + QStringLiteral("/.icons")));
    QVERIFY(paths.contains(fakeHome.path()
                           + QStringLiteral("/.local/share/flatpak/exports/share/icons")));
}

void AstreaIconProviderTest::applyPreservesSplitThemePriority()
{
    ScopedIconState state;
    ScopedEnvironment environment;
    QTemporaryDir dataHome;
    QTemporaryDir systemData;
    QTemporaryDir fakeHome;
    QVERIFY(dataHome.isValid());
    QVERIFY(systemData.isValid());
    QVERIFY(fakeHome.isValid());

    const QString userTheme = fakeHome.path()
        + QStringLiteral("/.icons/priority-theme");
    const QString systemTheme = systemData.path()
        + QStringLiteral("/icons/priority-theme");
    QVERIFY(QDir().mkpath(userTheme + QStringLiteral("/96x96/apps")));
    QVERIFY(QDir().mkpath(systemTheme + QStringLiteral("/48x48/apps")));

    const QByteArray userIndex =
        "[Icon Theme]\nName=User Priority Metadata\nDirectories=96x96/apps\n\n"
        "[96x96/apps]\nSize=96\nType=Fixed\nContext=Applications\n";
    const QByteArray systemIndex =
        "[Icon Theme]\nName=System Priority Metadata\nDirectories=48x48/apps\n\n"
        "[48x48/apps]\nSize=48\nType=Fixed\nContext=Applications\n";
    QVERIFY(writeIndexFile(userTheme, userIndex));
    QVERIFY(writeIndexFile(systemTheme, systemIndex));
    QVERIFY(writeRaster(userTheme + QStringLiteral("/96x96/apps/user-only-test.png"),
                        QSize(96, 96), QColor("#00aa00")));
    QVERIFY(writeRaster(userTheme + QStringLiteral("/96x96/apps/priority-test.png"),
                        QSize(96, 96), QColor("#00aa00")));
    QVERIFY(writeRaster(systemTheme + QStringLiteral("/48x48/apps/priority-test.png"),
                        QSize(48, 48), QColor("#aa0000")));

    environment.set("HOME", fakeHome.path().toUtf8());
    environment.set("XDG_DATA_HOME", dataHome.path().toUtf8());
    environment.set("XDG_DATA_DIRS", systemData.path().toUtf8());
    environment.set("ASTREA_ICON_THEME", "priority-theme");

    AstreaIconProvider provider;
    // The user-only probe identifies the first index.theme metadata source;
    // the duplicate icon identifies the content root selected by QIcon.
    const QString appliedTheme = AstreaIconTheme::apply();
    QCOMPARE(appliedTheme, QStringLiteral("priority-theme"));
    QSize size;
    const QPixmap userOnly = provider.requestPixmap(
        QStringLiteral("user-only-test?logicalSize=96&dpr=1"), &size, {});
    QCOMPARE(size, QSize(96, 96));
    QCOMPARE(userOnly.toImage().pixelColor(10, 20), QColor("#00aa00"));

    const QPixmap pixmap = provider.requestPixmap(
        QStringLiteral("priority-test?logicalSize=48&dpr=1"), &size, {});
    QCOMPARE(size, QSize(48, 48));
    QCOMPARE(pixmap.toImage().pixelColor(10, 20), QColor("#00aa00"));
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
