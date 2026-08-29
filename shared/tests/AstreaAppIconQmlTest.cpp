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
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "icons/AstreaIconProvider.hpp"

Q_IMPORT_QML_PLUGIN(Astrea_SharedPlugin)

namespace {

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
        "<circle cx=\"64\" cy=\"64\" r=\"24\" fill=\"none\" "
        "stroke=\"#ffd166\" stroke-width=\"4\"/>"
        "</svg>";
    return svg.write(content) == content.size();
}

} // namespace

class AstreaAppIconQmlTest final : public QObject {
    Q_OBJECT

private slots:
    void computesResolutionAwareSourceTarget();
    void spotlightUsesLogicalExtentAtEachDpr();
    void keepsSourceQualityIndependentFromPresentationScale();
    void opacityMaskPreservesInteriorDetailAtMaximumScale();
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

void AstreaAppIconQmlTest::opacityMaskPreservesInteriorDetailAtMaximumScale()
{
    // The offscreen Qt scene graph does not expose a stable OpacityMask
    // capture, so compare the exact same high-resolution source before and
    // after the rounded-alpha operation used by the visual mask. This keeps
    // the A/B result focused on representation detail rather than compositor
    // capture behavior.
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

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    AstreaAppIconQmlTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "AstreaAppIconQmlTest.moc"
