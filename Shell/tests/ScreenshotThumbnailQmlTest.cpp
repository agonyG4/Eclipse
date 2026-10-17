#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest/QtTest>

#include <cmath>

#include "platform/typhon/TyphonScreenCaptureClient.hpp"
#include "platform/typhon/TyphonSharedConnection.hpp"
#include "screenshot/ScreenshotController.hpp"
#include "screenshot/ScreenshotInputRegionBridge.hpp"
#include "screenshot/ScreenshotImageProvider.hpp"

class ScreenshotThumbnailQmlTest final : public QObject {
    Q_OBJECT

private slots:
    void thumbnailStartsHiddenAndTracksInputRegion();
    void thumbnailInteractionTransitions();
    void nativeDragThresholdUsesPlatformSetting();
    void nativeFileDragStartsImmediatelyAndRestoresOnCancel();
    void nativeFileDragExportsCanonicalCopyMetadata();
    void newCaptureInvalidatesDismissalAndDragSnapshotsStayFrozen();
    void previewStartsHiddenOpensExistingResultAndEscapes();
};

namespace {

struct ScreenshotQmlFixture {
    QTemporaryDir configDirectory;
    TyphonSharedConnection connection;
    TyphonScreenCaptureClient client{&connection};
    ScreenshotController controller{&client, nullptr,
        configDirectory.filePath(QStringLiteral("screenshot.json"))};
    ScreenshotInputRegionBridge inputRegion;
    QQmlApplicationEngine engine;

    ScreenshotQmlFixture()
    {
        engine.rootContext()->setContextProperty(QStringLiteral("ScreenshotController"),
                                                  &controller);
        engine.rootContext()->setContextProperty(QStringLiteral("ScreenshotInputRegion"),
                                                  &inputRegion);
        engine.addImageProvider(QStringLiteral("astrea-screenshot"),
                                new ScreenshotImageProvider(&controller));
    }
};

QObject *load(QQmlApplicationEngine &engine, const QString &name)
{
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Screenshot/")
                                 + name));
    if (component.status() != QQmlComponent::Ready)
        qFatal("QML component failed: %s", qPrintable(component.errorString()));
    QObject *object = component.create();
    if (!object)
        qFatal("QML object failed: %s", qPrintable(component.errorString()));
    return object;
}

} // namespace

void ScreenshotThumbnailQmlTest::thumbnailStartsHiddenAndTracksInputRegion()
{
    ScreenshotQmlFixture fixture;
    fixture.controller.m_image = QImage(160, 100, QImage::Format_RGBA8888);
    fixture.controller.m_imageGeneration = 1;
    fixture.controller.m_resultPath = QStringLiteral("/tmp/astrea-screenshot-test.png");
    std::unique_ptr<QObject> object(load(fixture.engine, QStringLiteral("ScreenshotThumbnail.qml")));
    auto *window = qobject_cast<QQuickWindow *>(object.get());
    QVERIFY(window);
    fixture.inputRegion.setWindow(window);
    QVERIFY(!window->isVisible());

    fixture.controller.m_thumbnailVisible = true;
    emit fixture.controller.thumbnailVisibleChanged();
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(object->property("lifecycleState").toString(),
                              QStringLiteral("visible"), 1000);
    auto *card = window->findChild<QQuickItem *>(QStringLiteral("thumbnailCard"));
    QVERIFY(card);
    QObject *visual = window->findChild<QObject *>(QStringLiteral("thumbnailVisual"));
    QVERIFY(visual);
    const qreal shadowPadding = visual->property("shadowPadding").toReal();
    QTRY_VERIFY_WITH_TIMEOUT(!window->mask().isEmpty(), 1000);
    const QPointF slot = object->property("slotPosition").toPointF();
    QVERIFY(std::abs(slot.x() + card->width() + shadowPadding + 20.0 - window->width()) < 1.0);
    QVERIFY(std::abs(slot.y() + card->height() + shadowPadding + 20.0 - window->height()) < 1.0);
    const qreal scaledWidth = card->width() * card->scale();
    const qreal scaledHeight = card->height() * card->scale();
    const QRectF visualRect(card->x() - (scaledWidth - card->width()) / 2,
                            card->y() - (scaledHeight - card->height()) / 2,
                            scaledWidth, scaledHeight);
    QVERIFY(std::abs(fixture.controller.thumbnailRect().x() - visualRect.x()) < 1.0);
    QVERIFY(std::abs(fixture.controller.thumbnailRect().y() - visualRect.y()) < 1.0);
    QVERIFY(std::abs(fixture.controller.thumbnailRect().width() - visualRect.width()) < 1.0);
    QVERIFY(std::abs(fixture.controller.thumbnailRect().height() - visualRect.height()) < 1.0);
    QVERIFY(window->mask().contains(QPoint(static_cast<int>(card->x() + 1),
                                           static_cast<int>(card->y() + 1))));
    QVERIFY(!window->mask().contains(QPoint(0, 0)));
}

void ScreenshotThumbnailQmlTest::nativeDragThresholdUsesPlatformSetting()
{
    ScreenshotQmlFixture fixture;
    fixture.controller.m_image = QImage(160, 100, QImage::Format_RGBA8888);
    fixture.controller.m_imageGeneration = 1;
    fixture.controller.m_resultPath = QStringLiteral("/tmp/astrea-screenshot-test.png");
    std::unique_ptr<QObject> object(load(fixture.engine, QStringLiteral("ScreenshotThumbnail.qml")));
    QCOMPARE(object->property("dragThreshold").toReal(), 8.0);
    QVERIFY(object->property("nativeDragDistance").toReal()
            >= object->property("dragThreshold").toReal());
}

void ScreenshotThumbnailQmlTest::nativeFileDragStartsImmediatelyAndRestoresOnCancel()
{
    ScreenshotQmlFixture fixture;
    fixture.controller.m_image = QImage(160, 100, QImage::Format_RGBA8888);
    fixture.controller.m_imageGeneration = 1;
    fixture.controller.m_resultPath =
        QString::fromUtf8("/tmp/Captura + #/截图 final.png");
    std::unique_ptr<QObject> object(load(fixture.engine, QStringLiteral("ScreenshotThumbnail.qml")));
    auto *window = qobject_cast<QQuickWindow *>(object.get());
    QVERIFY(window);
    fixture.inputRegion.setWindow(window);
    object->setProperty("nativeDragTestMode", true);

    fixture.controller.m_thumbnailVisible = true;
    emit fixture.controller.thumbnailVisibleChanged();
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(object->property("lifecycleState").toString(),
                              QStringLiteral("visible"), 1000);
    auto *card = window->findChild<QQuickItem *>(QStringLiteral("thumbnailCard"));
    QVERIFY(card);
    auto *idleTimer = window->findChild<QObject *>(QStringLiteral("idleTimer"));
    QVERIFY(idleTimer);
    object->setProperty("hovered", false);
    const QPoint center(static_cast<int>(card->x() + card->width() / 2),
                        static_cast<int>(card->y() + card->height() / 2));

    const int movement = static_cast<int>(object->property("nativeDragDistance").toReal()) + 4;
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, center);
    QTest::mouseMove(window, center + QPoint(movement, 0), 0);
    QTRY_COMPARE(object->property("nativeDragStartCount").toInt(), 1);
    QVERIFY(object->property("nativeFileDragging").toBool());
    QVERIFY(object->property("nativeDragConsumedPress").toBool());
    object->setProperty("hovered", false);
    QVERIFY(!idleTimer->property("running").toBool());
    QVERIFY(window->isVisible());
    QVERIFY(fixture.inputRegion.region().contains(
        QPoint(static_cast<int>(card->x() + card->width() / 2),
               static_cast<int>(card->y() + card->height() / 2))));

    QVERIFY(!fixture.controller.startQuickCapture());
    QVERIFY(fixture.controller.thumbnailVisible());
    QCOMPARE(fixture.controller.phase(), ScreenshotController::ScreenshotPhase::Idle);

    QVariant action;
    QVERIFY(QMetaObject::invokeMethod(object.get(), "finishNativeFileDrag",
                                      Q_RETURN_ARG(QVariant, action),
                                      Q_ARG(QVariant, QVariant::fromValue(int(Qt::IgnoreAction)))));
    QVERIFY(!object->property("nativeFileDragging").toBool());
    QVERIFY(object->property("nativeDragConsumedPress").toBool());
    QVERIFY(fixture.inputRegion.region().contains(
        QPoint(static_cast<int>(card->x() + card->width() / 2),
               static_cast<int>(card->y() + card->height() / 2))));
    QTRY_VERIFY_WITH_TIMEOUT(idleTimer->property("running").toBool(), 1000);

    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, center + QPoint(20, 0));
    QVERIFY(!fixture.controller.previewVisible());
    QVERIFY(fixture.controller.thumbnailVisible());
    QVERIFY(!object->property("nativeDragConsumedPress").toBool());

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, center);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, center);
    QTRY_VERIFY_WITH_TIMEOUT(fixture.controller.previewVisible(), 1000);
    fixture.controller.closePreview();
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(object->property("lifecycleState").toString(),
                              QStringLiteral("visible"), 1000);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, center);
    QTest::mouseMove(window, center + QPoint(movement, 0), 0);
    QTRY_COMPARE(object->property("nativeDragStartCount").toInt(), 2);
    object->setProperty("hovered", false);
    QVERIFY(!idleTimer->property("running").toBool());
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, center + QPoint(20, 0));
    QVariant copyAction;
    QVERIFY(QMetaObject::invokeMethod(object.get(), "finishNativeFileDrag",
                                      Q_RETURN_ARG(QVariant, copyAction),
                                      Q_ARG(QVariant, QVariant::fromValue(int(Qt::CopyAction)))));
    QCOMPARE(object->property("lifecycleState").toString(), QStringLiteral("dismissing"));
    QVERIFY(window->isVisible());
    QVERIFY(fixture.controller.thumbnailVisible());
    QTest::qWait(50);
    QVERIFY(window->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(!window->isVisible(), 1000);
    QVERIFY(!fixture.controller.thumbnailVisible());
    QVERIFY(!fixture.controller.previewVisible());
    QVERIFY(!idleTimer->property("running").toBool());
    QCOMPARE(fixture.inputRegion.region().boundingRect(), QRect(0, 0, 1, 1));
}

void ScreenshotThumbnailQmlTest::nativeFileDragExportsCanonicalCopyMetadata()
{
    ScreenshotQmlFixture fixture;
    fixture.controller.m_image = QImage(160, 100, QImage::Format_RGBA8888);
    fixture.controller.m_imageGeneration = 1;
    fixture.controller.m_resultPath =
        QString::fromUtf8("/tmp/Captura + #/截图 final.png");
    std::unique_ptr<QObject> object(load(fixture.engine, QStringLiteral("ScreenshotThumbnail.qml")));

    const QVariantMap mimeData = object->property("nativeDragMimeData").toMap();
    const QByteArray uriList = mimeData.value(QStringLiteral("text/uri-list")).toString().toUtf8();
    QVERIFY(uriList.endsWith("\r\n"));
    const QUrl uri = QUrl::fromEncoded(uriList.left(uriList.size() - 2), QUrl::StrictMode);
    QVERIFY(uri.isLocalFile());
    QCOMPARE(uri.toLocalFile(), fixture.controller.resultPath());
    QVERIFY(mimeData.contains(QStringLiteral("image/png")));
    const QByteArray png = mimeData.value(QStringLiteral("image/png")).toByteArray();
    QVERIFY(png.startsWith(QByteArray::fromHex("89504e470d0a1a0a")));
    QCOMPARE(QImage::fromData(png, "PNG").size(), fixture.controller.image().size());
    QCOMPARE(mimeData.value(QStringLiteral("text/plain")).toString(),
             fixture.controller.resultPath());
    QCOMPARE(object->property("nativeDragSupportedActions").toInt(),
             int(Qt::CopyAction));
    QCOMPARE(object->property("nativeDragProposedAction").toInt(),
             int(Qt::CopyAction));
    QVERIFY(object->property("nativeDragUsesManualStart").toBool());
}

void ScreenshotThumbnailQmlTest::newCaptureInvalidatesDismissalAndDragSnapshotsStayFrozen()
{
    ScreenshotQmlFixture fixture;
    fixture.controller.m_image = QImage(160, 100, QImage::Format_RGBA8888);
    fixture.controller.m_imageGeneration = 1;
    fixture.controller.m_resultPath = QStringLiteral("/tmp/astrea-screenshot-test.png");
    std::unique_ptr<QObject> object(load(fixture.engine, QStringLiteral("ScreenshotThumbnail.qml")));
    auto *window = qobject_cast<QQuickWindow *>(object.get());
    QVERIFY(window);
    fixture.inputRegion.setWindow(window);
    object->setProperty("nativeDragTestMode", true);

    fixture.controller.m_thumbnailVisible = true;
    emit fixture.controller.thumbnailVisibleChanged();
    QTRY_COMPARE_WITH_TIMEOUT(object->property("lifecycleState").toString(),
                              QStringLiteral("visible"), 1000);

    auto *timer = window->findChild<QObject *>(QStringLiteral("idleTimer"));
    auto *card = window->findChild<QQuickItem *>(QStringLiteral("thumbnailCard"));
    QVERIFY(timer);
    QVERIFY(card);
    timer->setProperty("interval", 1);
    QTRY_COMPARE_WITH_TIMEOUT(object->property("lifecycleState").toString(),
                              QStringLiteral("dismissing"), 1000);
    QVERIFY(window->isVisible());

    fixture.controller.m_image = QImage(90, 210, QImage::Format_RGBA8888);
    ++fixture.controller.m_imageGeneration;
    emit fixture.controller.imageChanged();
    QTRY_COMPARE_WITH_TIMEOUT(object->property("lifecycleState").toString(),
                              QStringLiteral("entering"), 1000);
    QVERIFY(window->isVisible());
    QVERIFY(fixture.controller.thumbnailVisible());
    QTRY_COMPARE_WITH_TIMEOUT(object->property("lifecycleState").toString(),
                              QStringLiteral("visible"), 1000);

    const qreal frozenWidth = card->width();
    const qreal frozenHeight = card->height();
    const QRectF frozenInputRect = fixture.controller.thumbnailRect();
    const QVariantMap frozenPayload = object->property("nativeDragMimeData").toMap();
    const QString frozenSource = fixture.controller.imageSource();
    object->setProperty("held", true);
    QVariant startResult;
    QVERIFY(QMetaObject::invokeMethod(object.get(), "startNativeFileDrag",
                                      Q_RETURN_ARG(QVariant, startResult)));
    QVERIFY(object->property("nativeFileDragging").toBool());

    fixture.controller.configuration()->setThumbnailWidth(480);
    fixture.controller.configuration()->setPosition(QStringLiteral("top-left"));
    fixture.controller.m_image = QImage(300, 70, QImage::Format_RGBA8888);
    ++fixture.controller.m_imageGeneration;
    emit fixture.controller.imageChanged();
    QVERIFY(object->property("pendingCaptureRefresh").toBool());
    QCOMPARE(card->width(), frozenWidth);
    QCOMPARE(card->height(), frozenHeight);
    QCOMPARE(fixture.controller.thumbnailRect(), frozenInputRect);
    QCOMPARE(object->property("nativeDragSource").toString(), frozenSource);
    QCOMPARE(object->property("nativeDragMimeData").toMap(), frozenPayload);

    QVariant cancelAction;
    QVERIFY(QMetaObject::invokeMethod(object.get(), "finishNativeFileDrag",
                                      Q_RETURN_ARG(QVariant, cancelAction),
                                      Q_ARG(QVariant, QVariant::fromValue(int(Qt::IgnoreAction)))));
    QTRY_COMPARE_WITH_TIMEOUT(object->property("lifecycleState").toString(),
                              QStringLiteral("entering"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(object->property("lifecycleState").toString(),
                              QStringLiteral("visible"), 1000);
    QVERIFY(!object->property("nativeFileDragging").toBool());
    QVERIFY(fixture.controller.thumbnailVisible());
    QVERIFY(card->width() > frozenWidth);
    QVERIFY(card->x() < window->width() / 2);
}

void ScreenshotThumbnailQmlTest::thumbnailInteractionTransitions()
{
    ScreenshotQmlFixture fixture;
    fixture.controller.m_image = QImage(160, 100, QImage::Format_RGBA8888);
    fixture.controller.m_imageGeneration = 1;
    fixture.controller.m_resultPath = QStringLiteral("/tmp/astrea-screenshot-test.png");
    QList<QQmlError> qmlWarnings;
    QObject::connect(&fixture.engine, &QQmlEngine::warnings, &fixture.engine,
                     [&qmlWarnings](const QList<QQmlError> &warnings) {
                         qmlWarnings.append(warnings);
                     });
    std::unique_ptr<QObject> object(load(fixture.engine, QStringLiteral("ScreenshotThumbnail.qml")));
    auto *window = qobject_cast<QQuickWindow *>(object.get());
    QVERIFY(window);
    fixture.inputRegion.setWindow(window);
    QVERIFY(!window->findChild<QQuickItem *>(QStringLiteral("dismissAffordance")));

    fixture.controller.m_thumbnailVisible = true;
    emit fixture.controller.thumbnailVisibleChanged();
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(object->property("lifecycleState").toString(),
                              QStringLiteral("visible"), 1000);
    auto *card = window->findChild<QQuickItem *>(QStringLiteral("thumbnailCard"));
    QVERIFY(card);
    auto *timer = window->findChild<QObject *>(QStringLiteral("idleTimer"));
    QVERIFY(timer);
    QCOMPARE(timer->property("interval").toInt(), 5000);
    object->setProperty("hovered", true);
    QVERIFY(!timer->property("running").toBool());
    object->setProperty("hovered", false);

    const QPoint center(static_cast<int>(card->x() + card->width() / 2),
                        static_cast<int>(card->y() + card->height() / 2));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);
    QTRY_VERIFY_WITH_TIMEOUT(fixture.controller.previewVisible(), 1000);
    QVERIFY(!fixture.controller.thumbnailVisible());
    fixture.controller.closePreview();
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(object->property("lifecycleState").toString(),
                              QStringLiteral("visible"), 1000);
    QVERIFY(fixture.controller.thumbnailVisible());
    QVERIFY(!fixture.controller.previewVisible());

    for (const QQmlError &warning : qmlWarnings) {
        QVERIFY2(!warning.toString().contains(QStringLiteral("undefined"), Qt::CaseInsensitive),
                 qPrintable(warning.toString()));
        QVERIFY2(!warning.toString().contains(QStringLiteral("sceneX")),
                 qPrintable(warning.toString()));
        QVERIFY2(!warning.toString().contains(QStringLiteral("sceneY")),
                 qPrintable(warning.toString()));
    }

    fixture.controller.m_thumbnailVisible = true;
    emit fixture.controller.thumbnailVisibleChanged();
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(object->property("lifecycleState").toString(),
                              QStringLiteral("visible"), 1000);
    timer->setProperty("interval", 1);
    QTRY_COMPARE_WITH_TIMEOUT(object->property("lifecycleState").toString(),
                              QStringLiteral("dismissing"), 1000);
    QVERIFY(window->isVisible());
    QVERIFY(fixture.controller.thumbnailVisible());
    QTRY_VERIFY_WITH_TIMEOUT(!window->isVisible(), 1000);
}

void ScreenshotThumbnailQmlTest::previewStartsHiddenOpensExistingResultAndEscapes()
{
    ScreenshotQmlFixture fixture;
    fixture.controller.m_image = QImage(160, 100, QImage::Format_RGBA8888);
    fixture.controller.m_imageGeneration = 1;
    fixture.controller.m_resultPath = QStringLiteral("/tmp/astrea-screenshot-test.png");
    int captureCount = 0;
    fixture.controller.m_captureRequest = [&captureCount](std::uint64_t) {
        ++captureCount;
        return true;
    };
    const QRectF sourceRect(500, 400, 240, 150);
    fixture.controller.setThumbnailRect(sourceRect);
    std::unique_ptr<QObject> object(load(fixture.engine, QStringLiteral("ScreenshotPreview.qml")));
    auto *window = qobject_cast<QQuickWindow *>(object.get());
    QVERIFY(window);
    QVERIFY(!window->isVisible());
    QCOMPARE(window->color(), QColor(Qt::transparent));
    auto *backdrop = window->findChild<QQuickItem *>(QStringLiteral("previewBackdrop"));
    auto *card = window->findChild<QQuickItem *>(QStringLiteral("previewCard"));
    auto *image = window->findChild<QQuickItem *>(QStringLiteral("previewImage"));
    auto *visual = window->findChild<QQuickItem *>(QStringLiteral("previewVisual"));
    QVERIFY(backdrop);
    QVERIFY(card);
    QVERIFY(image);
    QVERIFY(visual);

    fixture.controller.configuration()->setPreviewMaximumScale(0.9);
    fixture.controller.m_thumbnailVisible = true;
    fixture.controller.openPreview();
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(fixture.controller.previewVisible(), 1000);
    QVERIFY(!fixture.controller.thumbnailVisible());
    QCOMPARE(object->property("transitionSourceRect").toRectF(), sourceRect);
    const QRectF targetRect = object->property("targetRect").toRectF();
    QVERIFY(targetRect.width() > 0.0);
    QVERIFY(targetRect.height() > 0.0);
    QVERIFY(std::abs(targetRect.width() / targetRect.height() - 1.6) < 0.01);
    QVERIFY(std::abs(targetRect.center().x() - window->width() / 2.0) < 1.0);
    QVERIFY(std::abs(targetRect.center().y() - window->height() / 2.0) < 1.0);
    QVERIFY(targetRect.width() <= window->width() * 0.9 + 1.0);
    QVERIFY(targetRect.height() <= window->height() * 0.9 + 1.0);
    QVERIFY(qMax(targetRect.width() / window->width(),
                 targetRect.height() / window->height()) > 0.78);
    QCOMPARE(visual->property("cornerRadius").toReal(),
             fixture.controller.configuration()->cornerRadius());
    QVERIFY(visual->property("borderEnabled").toBool());
    QVERIFY(visual->property("shadowEnabled").toBool());
    QCOMPARE(captureCount, 0);

    QTRY_VERIFY_WITH_TIMEOUT(object->property("transitionProgress").toReal() > 0.999, 1000);
    QVERIFY(std::abs(card->x() - targetRect.x()) < 1.0);
    QVERIFY(std::abs(card->y() - targetRect.y()) < 1.0);
    QVERIFY(std::abs(card->width() - targetRect.width()) < 1.0);
    QVERIFY(std::abs(card->height() - targetRect.height()) < 1.0);
    QVERIFY(std::abs(backdrop->opacity() - 0.34) < 0.01);
    QVERIFY(std::abs(image->width() / image->height() - 1.6) < 0.01);

    auto *scope = window->findChild<QQuickItem *>(QStringLiteral("keyboardScope"));
    QVERIFY(scope);
    scope->forceActiveFocus();
    QTRY_VERIFY_WITH_TIMEOUT(scope->hasActiveFocus(), 1000);
    QTest::keyClick(window, Qt::Key_Escape);
    QVERIFY(fixture.controller.previewVisible());
    QVERIFY(window->isVisible());
    QVERIFY(object->property("closing").toBool());
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.previewVisible(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!window->isVisible(), 1000);
    QVERIFY(fixture.controller.thumbnailVisible());
    QCOMPARE(captureCount, 0);

    fixture.controller.openPreview();
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(object->property("transitionProgress").toReal() > 0.999, 1000);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller.previewVisible(), 1000);
    QVERIFY(fixture.controller.thumbnailVisible());
    QCOMPARE(captureCount, 0);
}

QTEST_MAIN(ScreenshotThumbnailQmlTest)
#include "ScreenshotThumbnailQmlTest.moc"
