#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QTest>

#include <memory>

#include "icons/AstreaIconProvider.hpp"

Q_IMPORT_QML_PLUGIN(Astrea_SharedPlugin)

class AstreaAppIconQmlTest final : public QObject {
    Q_OBJECT

private slots:
    void computesResolutionAwareSourceTarget();
    void spotlightUsesLogicalExtentAtEachDpr();
    void keepsSourceQualityIndependentFromPresentationScale();
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

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    AstreaAppIconQmlTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "AstreaAppIconQmlTest.moc"
