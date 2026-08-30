#include <QColor>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <memory>

#include "icons/AstreaIconProvider.hpp"

Q_IMPORT_QML_PLUGIN(Astrea_SharedPlugin)

namespace {

bool writeHighFrequencySvg(const QString &path, const bool light = false)
{
    QFile svg(path);
    if (!svg.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    const QByteArray content = light
        ? QByteArray(
              "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"128\" height=\"128\">"
              "<rect width=\"128\" height=\"128\" fill=\"#f4f1ea\"/>"
              "<path d=\"M0 0L128 128M-32 0L128 160M0 32L96 128M32 0L128 96M64 0L128 64\" "
              "stroke=\"#173f5f\" stroke-width=\"3\"/>"
              "<circle cx=\"64\" cy=\"64\" r=\"24\" fill=\"none\" "
              "stroke=\"#d1495b\" stroke-width=\"4\"/>"
              "</svg>")
        : QByteArray(
              "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"128\" height=\"128\">"
              "<rect width=\"128\" height=\"128\" fill=\"#101820\"/>"
              "<path d=\"M0 0L128 128M-32 0L128 160M0 32L96 128M32 0L128 96M64 0L128 64\" "
              "stroke=\"#00d084\" stroke-width=\"3\"/>"
              "<circle cx=\"64\" cy=\"64\" r=\"24\" fill=\"none\" "
              "stroke=\"#ffd166\" stroke-width=\"4\"/>"
              "</svg>");
    return svg.write(content) == content.size();
}

QRect captureBounds(QQuickWindow &window, QQuickItem *item, const QImage &capture)
{
    const QRectF logicalBounds = item->mapRectToItem(
        window.contentItem(), QRectF(0, 0, item->width(), item->height()));
    const qreal dpr = static_cast<qreal>(capture.width()) / window.width();
    const QRectF physicalBounds(logicalBounds.left() * dpr,
                                logicalBounds.top() * dpr,
                                logicalBounds.width() * dpr,
                                logicalBounds.height() * dpr);
    return physicalBounds.toAlignedRect().intersected(capture.rect());
}

double interiorContrast(const QImage &capture, const QRect &bounds)
{
    const QRect interior = bounds.adjusted(12, 12, -12, -12);
    if (interior.width() < 3 || interior.height() < 3)
        return 0.0;

    qint64 total = 0;
    qint64 samples = 0;
    for (int y = interior.top(); y < interior.bottom(); ++y) {
        for (int x = interior.left(); x < interior.right(); ++x) {
            const QColor current = capture.pixelColor(x, y);
            const QColor right = capture.pixelColor(x + 1, y);
            const QColor below = capture.pixelColor(x, y + 1);
            total += qAbs(current.red() - right.red())
                + qAbs(current.green() - right.green())
                + qAbs(current.blue() - right.blue());
            total += qAbs(current.red() - below.red())
                + qAbs(current.green() - below.green())
                + qAbs(current.blue() - below.blue());
            samples += 2;
        }
    }
    return samples > 0 ? static_cast<double>(total) / samples : 0.0;
}

bool hasPartialAlphaAtEdge(const QImage &capture, const QRect &bounds)
{
    const QRect edge = bounds.adjusted(-1, -1, 1, 1).intersected(capture.rect());
    for (int y = edge.top(); y <= edge.bottom(); ++y) {
        for (int x = edge.left(); x <= edge.right(); ++x) {
            const bool nearHorizontalEdge = y <= bounds.top() + 8
                || y >= bounds.bottom() - 8;
            const bool nearVerticalEdge = x <= bounds.left() + 8
                || x >= bounds.right() - 8;
            if (!nearHorizontalEdge && !nearVerticalEdge)
                continue;
            const int alpha = capture.pixelColor(x, y).alpha();
            if (alpha > 0 && alpha < 255)
                return true;
        }
    }
    return false;
}

bool isApproximately(const qreal actual, const qreal expected)
{
    return qAbs(actual - expected) < 0.001;
}

QObject *findObjectByName(QObject *root, const QString &name)
{
    if (!root)
        return nullptr;
    if (root->objectName() == name)
        return root;
    for (QObject *child : root->children()) {
        if (QObject *match = findObjectByName(child, name))
            return match;
    }
    return nullptr;
}

struct CapturedFrame {
    QImage image;
    QRect bounds;
};

CapturedFrame captureFrame(QQuickWindow &window, QQuickItem *item)
{
    window.requestUpdate();
    QTest::qWait(100);
    QCoreApplication::processEvents();
    CapturedFrame frame;
    frame.image = window.grabWindow();
    if (!frame.image.isNull())
        frame.bounds = captureBounds(window, item, frame.image);
    return frame;
}

QRect interiorBounds(const QRect &bounds)
{
    return bounds.adjusted(12, 12, -12, -12);
}

int maximumInteriorDifference(const QImage &first,
                              const QImage &second,
                              const QRect &bounds)
{
    int maximum = 0;
    const QRect interior = interiorBounds(bounds).intersected(first.rect())
                               .intersected(second.rect());
    for (int y = interior.top(); y <= interior.bottom(); ++y) {
        for (int x = interior.left(); x <= interior.right(); ++x) {
            const QColor a = first.pixelColor(x, y);
            const QColor b = second.pixelColor(x, y);
            maximum = qMax(maximum,
                           qAbs(a.red() - b.red())
                               + qAbs(a.green() - b.green())
                               + qAbs(a.blue() - b.blue())
                               + qAbs(a.alpha() - b.alpha()));
        }
    }
    return maximum;
}

bool hasOpaqueInterior(const QImage &capture, const QRect &bounds)
{
    const QRect interior = interiorBounds(bounds).intersected(capture.rect());
    for (int y = interior.top(); y <= interior.bottom(); ++y) {
        for (int x = interior.left(); x <= interior.right(); ++x) {
            if (capture.pixelColor(x, y).alpha() != 255)
                return false;
        }
    }
    return !interior.isEmpty();
}

bool transparentCornersAreClean(const QImage &capture, const QRect &bounds)
{
    const QPoint topLeft = bounds.topLeft() + QPoint(1, 1);
    const QPoint topRight = bounds.topRight() + QPoint(-1, 1);
    const QPoint bottomLeft = bounds.bottomLeft() + QPoint(1, -1);
    const QPoint bottomRight = bounds.bottomRight() + QPoint(-1, -1);
    for (const QPoint point : {topLeft, topRight, bottomLeft, bottomRight}) {
        if (!capture.rect().contains(point))
            return false;
        const QColor pixel = capture.pixelColor(point);
        if (pixel.alpha() != 0 || pixel.red() != 0 || pixel.green() != 0
            || pixel.blue() != 0) {
            return false;
        }
    }
    return true;
}

std::unique_ptr<QQuickItem> createLegacyOpacityMaskIcon(QQmlEngine &engine,
                                                         const QString &source,
                                                         const int sourcePixels)
{
    QQmlComponent component(&engine);
    component.setData(R"qml(
        import QtQuick
        import Qt5Compat.GraphicalEffects

        Item {
            id: root
            objectName: "legacyOpacityMaskIcon"
            property url source
            property int sourcePixelSize: 1
            property int iconRadius: 7
            property bool ready: iconImage.status === Image.Ready

            Image {
                id: iconImage
                anchors.fill: parent
                source: root.source
                sourceSize: Qt.size(root.sourcePixelSize, root.sourcePixelSize)
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                asynchronous: true
                visible: status === Image.Ready && root.iconRadius <= 0
            }

            OpacityMask {
                anchors.fill: iconImage
                source: iconImage
                maskSource: Rectangle {
                    width: iconImage.width
                    height: iconImage.height
                    radius: root.iconRadius > 0 ? root.iconRadius : 0
                    visible: false
                }
                visible: iconImage.status === Image.Ready && root.iconRadius > 0
            }
        }
    )qml", QUrl(QStringLiteral("qrc:/LegacyOpacityMaskIcon.qml")));
    if (!component.isReady())
        return {};
    auto icon = std::unique_ptr<QQuickItem>(qobject_cast<QQuickItem *>(component.create()));
    if (!icon)
        return {};
    icon->setProperty("source", QUrl::fromLocalFile(source));
    icon->setProperty("sourcePixelSize", sourcePixels);
    return icon;
}

} // namespace

class AstreaAppIconQmlTest final : public QObject {
    Q_OBJECT

private slots:
    void computesResolutionAwareSourceTarget();
    void spotlightUsesLogicalExtentAtEachDpr();
    void keepsSourceQualityIndependentFromPresentationScale();
    void roundedAlphaMathPreservesInteriorDetail();
    void opacityMaskPreservesInteriorDetailAtMaximumScale();
    void roundedPathPreservesFallbackAndDirectImage();
};

static std::unique_ptr<QQuickItem> createIcon(QQmlEngine &engine)
{
    engine.addImportPath(QStringLiteral(ASTREA_QML_IMPORT_PATH));
    engine.addImageProvider(QStringLiteral("astrea-icon"), new AstreaIconProvider);
    QQmlComponent component(&engine,
                            QUrl::fromLocalFile(QStringLiteral(ASTREA_SHARED_ICON_QML)));
    if (!component.isReady())
        return {};
    return std::unique_ptr<QQuickItem>(qobject_cast<QQuickItem *>(component.create()));
}

void AstreaAppIconQmlTest::computesResolutionAwareSourceTarget()
{
    QQmlEngine engine;
    auto icon = createIcon(engine);
    QVERIFY(icon);
    icon->setProperty("iconName", QStringLiteral("test-icon"));
    icon->setProperty("iconSize", 48);
    icon->setProperty("maximumPresentationScale", 2.0);
    icon->setProperty("devicePixelRatioOverride", 1.0);
    QCoreApplication::processEvents();
    QCOMPARE(icon->property("effectiveMaximumLogicalSize").toInt(), 96);
    QCOMPARE(icon->property("effectiveSourcePixelSize").toInt(), 96);

    icon->setProperty("devicePixelRatioOverride", 2.0);
    QCoreApplication::processEvents();
    QCOMPARE(icon->property("effectiveMaximumLogicalSize").toInt(), 96);
    QCOMPARE(icon->property("effectiveSourcePixelSize").toInt(), 192);

    icon->setProperty("iconSize", 64);
    icon->setProperty("maximumPresentationLogicalSize", 0.0);
    icon->setProperty("maximumPresentationScale", 2.0);
    QCoreApplication::processEvents();
    QCOMPARE(icon->property("effectiveMaximumLogicalSize").toInt(), 128);
    QCOMPARE(icon->property("effectiveSourcePixelSize").toInt(), 256);

    icon->setProperty("maximumPresentationLogicalSize", 64.0);
    QCoreApplication::processEvents();
    QCOMPARE(icon->property("effectiveMaximumLogicalSize").toInt(), 64);
    QCOMPARE(icon->property("effectiveSourcePixelSize").toInt(), 128);

    icon->setProperty("maximumPresentationLogicalSize", 48.0 * 1.6);
    icon->setProperty("devicePixelRatioOverride", 1.5);
    QCoreApplication::processEvents();
    QCOMPARE(icon->property("effectiveMaximumLogicalSize").toInt(), 77);
    QCOMPARE(icon->property("effectiveSourcePixelSize").toInt(), 116);

    const QString source = icon->property("resolvedSource").toString();
    QVERIFY(source.contains(QStringLiteral("logicalSize=77")));
    QVERIFY(source.contains(QStringLiteral("dpr=1.500")));
    QVERIFY(source.contains(QStringLiteral("pixelSize=116")));
}

void AstreaAppIconQmlTest::spotlightUsesLogicalExtentAtEachDpr()
{
    QQmlEngine engine;
    auto icon = createIcon(engine);
    QVERIFY(icon);
    icon->setProperty("maximumPresentationLogicalSize", 40.0);
    icon->setProperty("devicePixelRatioOverride", 1.0);
    QCoreApplication::processEvents();
    QCOMPARE(icon->property("effectiveMaximumLogicalSize").toInt(), 40);
    QCOMPARE(icon->property("effectiveSourcePixelSize").toInt(), 40);

    icon->setProperty("devicePixelRatioOverride", 2.0);
    QCoreApplication::processEvents();
    QCOMPARE(icon->property("effectiveMaximumLogicalSize").toInt(), 40);
    QCOMPARE(icon->property("effectiveSourcePixelSize").toInt(), 80);
}

void AstreaAppIconQmlTest::keepsSourceQualityIndependentFromPresentationScale()
{
    QQmlEngine engine;
    auto icon = createIcon(engine);
    QVERIFY(icon);
    icon->setProperty("iconName", QStringLiteral("test-icon"));
    icon->setProperty("iconSize", 48);
    icon->setProperty("maximumPresentationLogicalSize", 84.0);
    icon->setProperty("devicePixelRatioOverride", 2.0);
    QCoreApplication::processEvents();

    const QString source = icon->property("resolvedSource").toString();
    const int sourcePixels = icon->property("effectiveSourcePixelSize").toInt();
    icon->setProperty("scale", 1.6);
    QCoreApplication::processEvents();

    QCOMPARE(icon->property("resolvedSource").toString(), source);
    QCOMPARE(icon->property("effectiveSourcePixelSize").toInt(), sourcePixels);
    QCOMPARE(icon->property("scale").toReal(), 1.6);
}

void AstreaAppIconQmlTest::roundedAlphaMathPreservesInteriorDetail()
{
    // This is a deterministic source-buffer check of the rounded-alpha
    // operation. It does not execute the QML OpacityMask effect.
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString svgPath = temporary.path() + QStringLiteral("/mask-ab.svg");
    QVERIFY(writeHighFrequencySvg(svgPath));

    QImageReader reader(svgPath);
    constexpr qreal presentationScale = 1.6;
    constexpr int logicalSize = 96;
    const QSize physicalSize(qRound(logicalSize * presentationScale),
                              qRound(logicalSize * presentationScale));
    reader.setScaledSize(physicalSize);
    const QImage unmasked = reader.read()
                                .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QVERIFY(!unmasked.isNull());

    const qreal radius = 7.0 * presentationScale;
    QImage masked = unmasked;
    const auto insideRoundedRect = [&](qreal x, qreal y) {
        const qreal width = physicalSize.width();
        const qreal height = physicalSize.height();
        if (x >= radius && x <= width - radius)
            return true;
        if (y >= radius && y <= height - radius)
            return true;
        const qreal cornerX = x < radius ? radius : width - radius;
        const qreal cornerY = y < radius ? radius : height - radius;
        const qreal dx = x - cornerX;
        const qreal dy = y - cornerY;
        return dx * dx + dy * dy <= radius * radius;
    };
    for (int y = 0; y < physicalSize.height(); ++y) {
        for (int x = 0; x < physicalSize.width(); ++x) {
            int coveredSamples = 0;
            for (int sampleY = 0; sampleY < 4; ++sampleY) {
                for (int sampleX = 0; sampleX < 4; ++sampleX) {
                    coveredSamples += insideRoundedRect(
                        x + (sampleX + 0.5) / 4.0,
                        y + (sampleY + 0.5) / 4.0) ? 1 : 0;
                }
            }
            QColor pixel = masked.pixelColor(x, y);
            pixel.setAlpha(pixel.alpha() * coveredSamples / 16);
            masked.setPixelColor(x, y, pixel);
        }
    }

    int maximumInteriorDifference = 0;
    int interiorRange = 0;
    QColor firstInterior;
    for (int y = 16; y < physicalSize.height() - 16; ++y) {
        for (int x = 16; x < physicalSize.width() - 16; ++x) {
            const QColor a = unmasked.pixelColor(x, y);
            const QColor b = masked.pixelColor(x, y);
            maximumInteriorDifference = qMax(maximumInteriorDifference,
                qAbs(a.red() - b.red()) + qAbs(a.green() - b.green())
                    + qAbs(a.blue() - b.blue()) + qAbs(a.alpha() - b.alpha()));
            if (x == 16 && y == 16)
                firstInterior = a;
            interiorRange = qMax(interiorRange,
                                 qAbs(a.red() - firstInterior.red())
                                     + qAbs(a.green() - firstInterior.green())
                                     + qAbs(a.blue() - firstInterior.blue()));
        }
    }
    QCOMPARE(maximumInteriorDifference, 0);
    QVERIFY(interiorRange > 20);
    QVERIFY(unmasked.pixelColor(0, 0).alpha() > 0);
    QCOMPARE(masked.pixelColor(0, 0).alpha(), 0);

    bool hasAntialiasedEdge = false;
    for (int y = 0; y < physicalSize.height() && !hasAntialiasedEdge; ++y) {
        for (int x = 0; x < physicalSize.width(); ++x) {
            const int alpha = masked.pixelColor(x, y).alpha();
            if (alpha > 0 && alpha < 255) {
                hasAntialiasedEdge = true;
                break;
            }
        }
    }
    QVERIFY(hasAntialiasedEdge);
}

void AstreaAppIconQmlTest::opacityMaskPreservesInteriorDetailAtMaximumScale()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    constexpr qreal presentationScale = 1.6;
    constexpr int logicalSize = 96;

    for (const bool lightArtwork : {false, true}) {
        const QString suffix = lightArtwork ? QStringLiteral("-light")
                                            : QStringLiteral("-dark");
        const QString svgPath = temporary.path() + QStringLiteral("/mask-qml-ab")
            + suffix + QStringLiteral(".svg");
        QVERIFY(writeHighFrequencySvg(svgPath, lightArtwork));

        QQmlEngine engine;
        auto modern = createIcon(engine);
        QVERIFY(modern);
        modern->setWidth(logicalSize);
        modern->setHeight(logicalSize);
        modern->setX(72);
        modern->setY(72);
        modern->setProperty("iconPath", QUrl::fromLocalFile(svgPath).toString());
        modern->setProperty("iconSize", logicalSize);
        modern->setProperty("maximumPresentationScale", presentationScale);
        // The benchmark must use the real Screen/QQuickWindow DPR.
        modern->setProperty("devicePixelRatioOverride", 0.0);
        modern->setProperty("showFallbackText", false);
        modern->setProperty("scale", presentationScale);
        modern->setProperty("iconRadius", 0);

        auto legacy = createLegacyOpacityMaskIcon(
            engine, svgPath, logicalSize);
        QVERIFY(legacy);
        legacy->setWidth(logicalSize);
        legacy->setHeight(logicalSize);
        legacy->setX(72);
        legacy->setY(72);
        legacy->setScale(presentationScale);
        legacy->setProperty("iconRadius", 0);
        legacy->setVisible(false);

        QQuickWindow window;
        window.setColor(Qt::transparent);
        window.resize(240, 240);
        modern->setParentItem(window.contentItem());
        legacy->setParentItem(window.contentItem());
        window.show();

        QVERIFY(window.screen());
        const qreal screenDpr = window.screen()->devicePixelRatio();
        const qreal windowDpr = window.devicePixelRatio();
        const qreal effectiveWindowDpr = window.effectiveDevicePixelRatio();
        QTRY_VERIFY_WITH_TIMEOUT(modern->property("ready").toBool(), 3000);
        const qreal qmlDpr = modern->property("effectiveDevicePixelRatio").toReal();
        qInfo() << "Rounded icon A/B" << (lightArtwork ? "light" : "dark")
                << "Screen DPR" << screenDpr << "window DPR" << windowDpr
                << "effective window DPR" << effectiveWindowDpr
                << "QML DPR" << qmlDpr;
        QVERIFY(isApproximately(screenDpr, windowDpr));
        QVERIFY(isApproximately(windowDpr, effectiveWindowDpr));
        QVERIFY(isApproximately(effectiveWindowDpr, qmlDpr));

        const int expectedSourcePixels = qCeil(qCeil(logicalSize * presentationScale)
                                                * effectiveWindowDpr);
        const int sourcePixels = modern->property("effectiveSourcePixelSize").toInt();
        QCOMPARE(modern->property("effectiveMaximumLogicalSize").toInt(), 154);
        QCOMPARE(sourcePixels, expectedSourcePixels);
        QVERIFY(sourcePixels >= 154);
        const QString source = modern->property("resolvedSource").toString();
        QVERIFY(source.contains(svgPath));
        QVERIFY(!findObjectByName(modern.get(), QStringLiteral("roundedIconEffect")));

        const CapturedFrame unmasked = captureFrame(window, modern.get());
        if (unmasked.image.isNull() || unmasked.bounds.width() <= 32
            || unmasked.bounds.height() <= 32) {
            QSKIP("Qt Quick did not produce a capturable unmasked frame on this platform");
        }

        legacy->setProperty("sourcePixelSize", sourcePixels);
        legacy->setProperty("iconRadius", 7);
        legacy->setVisible(true);
        modern->setVisible(false);
        QCOMPARE(legacy->property("source").toUrl(), QUrl::fromLocalFile(svgPath));
        QCOMPARE(legacy->property("sourcePixelSize").toInt(), sourcePixels);
        QTRY_VERIFY_WITH_TIMEOUT(legacy->property("ready").toBool(), 3000);
        const CapturedFrame legacyFrame = captureFrame(window, legacy.get());
        if (legacyFrame.image.isNull() || legacyFrame.bounds.width() <= 32
            || legacyFrame.bounds.height() <= 32) {
            QSKIP("Qt Quick did not produce a capturable legacy OpacityMask frame on this platform");
        }

        legacy->setVisible(false);
        modern->setVisible(true);
        modern->setProperty("iconRadius", 7);
        QTRY_VERIFY_WITH_TIMEOUT(
            findObjectByName(modern.get(), QStringLiteral("roundedIconEffect")), 3000);
        QObject *effect = findObjectByName(modern.get(), QStringLiteral("roundedIconEffect"));
        QVERIFY(effect);
        const QVariant hasProxySource = effect->property("hasProxySource");
        const bool proxySource = hasProxySource.isValid() && hasProxySource.toBool();
        const QString proxyState = hasProxySource.isValid()
            ? QString::number(proxySource) : QStringLiteral("unavailable");
        qInfo() << "rounded path hasProxySource"
                << proxyState;
        if (hasProxySource.isValid()) {
            QVERIFY2(!proxySource,
                     "MultiEffect unexpectedly flattened the Image through a proxy source");
        }
        const CapturedFrame roundedFrame = captureFrame(window, modern.get());
        if (roundedFrame.image.isNull() || roundedFrame.bounds.width() <= 32
            || roundedFrame.bounds.height() <= 32) {
            QSKIP("Qt Quick did not produce a capturable rounded frame on this platform");
        }

        QCOMPARE(modern->property("resolvedSource").toString(), source);
        QCOMPARE(modern->property("effectiveSourcePixelSize").toInt(), sourcePixels);
        QCOMPARE(modern->property("scale").toReal(), presentationScale);

        const QRect sharedBounds = unmasked.bounds.intersected(legacyFrame.bounds)
                                      .intersected(roundedFrame.bounds);
        QVERIFY(sharedBounds.width() > 32);
        QVERIFY(sharedBounds.height() > 32);
        const double unmaskedContrast = interiorContrast(unmasked.image, sharedBounds);
        const double legacyContrast = interiorContrast(legacyFrame.image, sharedBounds);
        const double roundedContrast = interiorContrast(roundedFrame.image, sharedBounds);
        qInfo() << "Rounded icon pre-metric" << (lightArtwork ? "light" : "dark")
                << "unmasked" << unmaskedContrast << "legacy" << legacyContrast
                << "new" << roundedContrast;
        if (unmaskedContrast <= 1.0 || legacyContrast <= 1.0 || roundedContrast <= 1.0) {
            if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
                QSKIP("Qt Quick effect output is not capturable on the offscreen platform");
            QFAIL("Qt Quick rounded-path output did not produce a measurable interior");
        }

        const double legacyRatio = legacyContrast / unmaskedContrast;
        const double roundedRatio = roundedContrast / unmaskedContrast;
        const int legacyMaximumInteriorDifference = maximumInteriorDifference(
            unmasked.image, legacyFrame.image, sharedBounds);
        const int roundedMaximumInteriorDifference = maximumInteriorDifference(
            unmasked.image, roundedFrame.image, sharedBounds);
        qInfo() << "Rounded icon A/B" << (lightArtwork ? "light" : "dark")
                << "source pixel extent" << sourcePixels << "Screen DPR" << screenDpr
                << "window DPR" << windowDpr << "effective window DPR"
                << effectiveWindowDpr << "unmasked contrast" << unmaskedContrast
                << "legacy OpacityMask contrast" << legacyContrast
                << "new rounded contrast" << roundedContrast
                << "legacy contrast ratio" << legacyRatio
                << "new contrast ratio" << roundedRatio
                << "legacy maximum interior difference"
                << legacyMaximumInteriorDifference
                << "new maximum interior difference"
                << roundedMaximumInteriorDifference << "hasProxySource" << proxyState;

        // The 12-pixel physical inset excludes the rounded boundary and all
        // antialiased corner samples from the interior metric. This threshold
        // is intentionally strict enough to fail when the legacy effect is
        // restored, while allowing normal shader sampling noise.
        QVERIFY2(roundedRatio >= 0.98,
                 "the source-preserving rounded path softened interior detail");
        QVERIFY2(roundedRatio >= legacyRatio + 0.005,
                 "the new rounded path did not materially beat legacy OpacityMask");
        QVERIFY(roundedMaximumInteriorDifference < legacyMaximumInteriorDifference);

        const bool legacyCornersClean = transparentCornersAreClean(
            legacyFrame.image, legacyFrame.bounds);
        const bool legacyPartialEdge = hasPartialAlphaAtEdge(
            legacyFrame.image, legacyFrame.bounds);
        const bool legacyOpaqueInterior = hasOpaqueInterior(
            legacyFrame.image, legacyFrame.bounds);
        const bool roundedCornersClean = transparentCornersAreClean(
            roundedFrame.image, roundedFrame.bounds);
        const bool roundedPartialEdge = hasPartialAlphaAtEdge(
            roundedFrame.image, roundedFrame.bounds);
        const bool roundedOpaqueInterior = hasOpaqueInterior(
            roundedFrame.image, roundedFrame.bounds);
        qInfo() << "Rounded icon edge alpha: legacy corners clean"
                << legacyCornersClean << "legacy partial edge" << legacyPartialEdge
                << "legacy opaque interior" << legacyOpaqueInterior
                << "new corners clean" << roundedCornersClean
                << "new partial edge" << roundedPartialEdge
                << "new opaque interior" << roundedOpaqueInterior;
        QVERIFY(roundedCornersClean);
        QVERIFY(roundedPartialEdge);
        QVERIFY(roundedOpaqueInterior);
    }
}

void AstreaAppIconQmlTest::roundedPathPreservesFallbackAndDirectImage()
{
    QQmlEngine engine;
    auto icon = createIcon(engine);
    QVERIFY(icon);
    icon->setProperty("iconName", QStringLiteral("astrea-icon-that-does-not-exist"));
    icon->setProperty("iconSize", 96);
    icon->setProperty("maximumPresentationScale", 1.6);
    icon->setProperty("appName", QStringLiteral("Fallback App"));
    icon->setProperty("showFallbackText", true);
    icon->setProperty("iconRadius", 0);
    QCoreApplication::processEvents();

    QVERIFY(!findObjectByName(icon.get(), QStringLiteral("roundedIconEffect")));
    icon->setProperty("iconRadius", 7);
    QTRY_VERIFY_WITH_TIMEOUT(!icon->property("ready").toBool(), 3000);
    QVERIFY(!findObjectByName(icon.get(), QStringLiteral("roundedIconEffect")));
    QObject *fallback = findObjectByName(icon.get(), QStringLiteral("fallbackSurface"));
    QVERIFY(fallback);
    QVERIFY(fallback->property("visible").toBool());
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    AstreaAppIconQmlTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "AstreaAppIconQmlTest.moc"
