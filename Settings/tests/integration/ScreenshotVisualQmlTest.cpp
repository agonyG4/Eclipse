#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class ScreenshotVisualQmlTest final : public QObject {
    Q_OBJECT

private slots:
    void sharedComponentLoadsWithBoundedGeometry();
    void imageCornersAreActuallyTransparent();
};

namespace {

static QUrl componentUrl()
{
    return QUrl::fromLocalFile(QStringLiteral(ASTREA_ECLIPSE_SOURCE_DIR)
                               + QStringLiteral("/shared/qml/ScreenshotVisual.qml"));
}

static std::unique_ptr<QObject> createVisual(QQmlEngine &engine)
{
    QQmlComponent component(&engine, componentUrl());
    if (component.status() != QQmlComponent::Ready)
        return {};
    return std::unique_ptr<QObject>(component.create());
}

} // namespace

void ScreenshotVisualQmlTest::sharedComponentLoadsWithBoundedGeometry()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, componentUrl());

    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY(object);
    auto *visual = qobject_cast<QQuickItem *>(object.get());
    QVERIFY(visual);

    QCOMPARE(visual->property("cornerRadius").toReal(), 10.0);
    QVERIFY(visual->property("borderEnabled").toBool());
    QCOMPARE(visual->property("borderWidth").toReal(), 1.0);
    QVERIFY(visual->property("shadowEnabled").toBool());
    QVERIFY(visual->property("shadowPadding").toReal() > 0.0);
    QVERIFY(!visual->clip());
}

void ScreenshotVisualQmlTest::imageCornersAreActuallyTransparent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QImage source(64, 36, QImage::Format_RGBA8888);
    source.fill(QColor(28, 190, 90, 255));
    const QString imagePath = directory.filePath(QStringLiteral("source.png"));
    QVERIFY(source.save(imagePath));

    QQmlEngine engine;
    auto object = createVisual(engine);
    QVERIFY(object);
    auto *visual = qobject_cast<QQuickItem *>(object.get());
    QVERIFY(visual);
    visual->setProperty("source", QUrl::fromLocalFile(imagePath));
    visual->setProperty("imageWidth", 1600.0);
    visual->setProperty("imageHeight", 900.0);
    visual->setProperty("cornerRadius", 0.0);
    visual->setProperty("borderEnabled", false);
    visual->setProperty("shadowEnabled", false);

    QQuickWindow window;
    window.setColor(Qt::transparent);
    window.resize(1640, 940);
    QQmlComponent markerComponent(&engine);
    markerComponent.setData(QByteArrayLiteral("import QtQuick\nRectangle { width: 10; height: 10; color: 'red' }"), {});
    std::unique_ptr<QObject> markerObject(markerComponent.create());
    QVERIFY2(markerObject, qPrintable(markerComponent.errorString()));
    auto *marker = qobject_cast<QQuickItem *>(markerObject.get());
    QVERIFY(marker);
    marker->setParentItem(window.contentItem());
    marker->setPosition(QPointF(1620.0, 920.0));
    visual->setParentItem(window.contentItem());
    visual->setPosition(QPointF(12.0, 12.0));
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isVisible(), 1000);
    auto *image = visual->findChild<QObject *>(QStringLiteral("screenshotImage"));
    QVERIFY(image);
    QTRY_COMPARE_WITH_TIMEOUT(image->property("status").toInt(), 1, 1000);
    window.requestUpdate();
    QTest::qWait(100);
    QCoreApplication::processEvents();

    const qreal ratio = window.devicePixelRatio();
    const auto pixel = [ratio](const QImage &rendered, qreal x, qreal y) {
        return rendered.pixelColor(qRound(x * ratio), qRound(y * ratio));
    };
    QImage rendered = window.grabWindow();
    QVERIFY(!rendered.isNull());
    // Qt's offscreen QPA does not render MultiEffect masks consistently.
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("The offscreen QPA cannot validate Qt Quick mask pixels");
    if (pixel(rendered, 1625.0, 925.0).alpha() < 240)
        QSKIP("The selected QPA backend did not render the test window");
    QVERIFY(pixel(rendered, 1625.0, 925.0).red() > 200);
    QVERIFY(pixel(rendered, 13.0, 13.0).alpha() > 240);
    QVERIFY(pixel(rendered, 812.0, 462.0).alpha() > 240);

    visual->setProperty("cornerRadius", 20.0);
    QTRY_VERIFY_WITH_TIMEOUT(visual->property("cornerRadius").toReal() == 20.0, 500);
    rendered = window.grabWindow();
    QVERIFY(!rendered.isNull());
    QVERIFY(pixel(rendered, 13.0, 13.0).alpha() < 20);
    QVERIFY(pixel(rendered, 812.0, 462.0).alpha() > 240);

    visual->setProperty("shadowEnabled", true);
    QTRY_VERIFY_WITH_TIMEOUT(visual->property("shadowPadding").toReal() > 0.0, 500);
    visual->setProperty("shadowEnabled", false);
    QTRY_COMPARE_WITH_TIMEOUT(visual->property("shadowPadding").toReal(), 0.0, 500);
    rendered = window.grabWindow();
    QVERIFY(pixel(rendered, 13.0, 13.0).alpha() < 20);
}

QTEST_MAIN(ScreenshotVisualQmlTest)

#include "ScreenshotVisualQmlTest.moc"
