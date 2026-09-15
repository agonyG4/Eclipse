#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
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
    void gesturePolicyDistinguishesClickReturnAndDismiss();
    void nativeFileDragArmsSeparatelyAndRestoresOnCancel();
    void nativeFileDragExportsCanonicalCopyMetadata();
    void previewStartsHiddenOpensExistingResultAndEscapes();
};

namespace {

struct ScreenshotQmlFixture {
    TyphonSharedConnection connection;
    TyphonScreenCaptureClient client{&connection};
    ScreenshotController controller{&client};
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
    auto *card = window->findChild<QQuickItem *>(QStringLiteral("thumbnailCard"));
    QVERIFY(card);
    QTRY_VERIFY_WITH_TIMEOUT(!window->mask().isEmpty(), 1000);
    const QPointF slot = object->property("slotPosition").toPointF();
    QVERIFY(std::abs(slot.x() + card->width() + 20.0 - window->width()) < 1.0);
    QVERIFY(std::abs(slot.y() + card->height() + 20.0 - window->height()) < 1.0);
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

void ScreenshotThumbnailQmlTest::gesturePolicyDistinguishesClickReturnAndDismiss()
{
    ScreenshotQmlFixture fixture;
    fixture.controller.m_image = QImage(160, 100, QImage::Format_RGBA8888);
    fixture.controller.m_imageGeneration = 1;
    fixture.controller.m_resultPath = QStringLiteral("/tmp/astrea-screenshot-test.png");
    std::unique_ptr<QObject> object(load(fixture.engine, QStringLiteral("ScreenshotThumbnail.qml")));
    QCOMPARE(object->property("dragThreshold").toReal(), 8.0);
    QCOMPARE(object->property("dismissThreshold").toReal(), 96.0);
    QVariant decision;
    QVERIFY(QMetaObject::invokeMethod(object.get(), "decideGesture",
                                      Q_RETURN_ARG(QVariant, decision),
                                      Q_ARG(QVariant, QVariant(0.0)),
                                      Q_ARG(QVariant, QVariant(0.0))));
    QCOMPARE(decision.toString(), QStringLiteral("click"));
    QVERIFY(QMetaObject::invokeMethod(object.get(), "decideGesture",
                                      Q_RETURN_ARG(QVariant, decision),
                                      Q_ARG(QVariant, QVariant(20.0)),
                                      Q_ARG(QVariant, QVariant(0.0))));
    QCOMPARE(decision.toString(), QStringLiteral("return"));
    QVERIFY(QMetaObject::invokeMethod(object.get(), "decideGesture",
                                      Q_RETURN_ARG(QVariant, decision),
                                      Q_ARG(QVariant, QVariant(96.0)),
                                      Q_ARG(QVariant, QVariant(0.0))));
    QCOMPARE(decision.toString(), QStringLiteral("dismiss"));
}

void ScreenshotThumbnailQmlTest::nativeFileDragArmsSeparatelyAndRestoresOnCancel()
{
    ScreenshotQmlFixture fixture;
    fixture.controller.m_image = QImage(160, 100, QImage::Format_RGBA8888);
    fixture.controller.m_imageGeneration = 1;
    fixture.controller.m_resultPath =
        QString::fromUtf8("/tmp/Captura de tela/截图 final.png");
    std::unique_ptr<QObject> object(load(fixture.engine, QStringLiteral("ScreenshotThumbnail.qml")));
    auto *window = qobject_cast<QQuickWindow *>(object.get());
    QVERIFY(window);
    fixture.inputRegion.setWindow(window);
    object->setProperty("nativeDragTestMode", true);

    fixture.controller.m_thumbnailVisible = true;
    emit fixture.controller.thumbnailVisibleChanged();
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
    auto *card = window->findChild<QQuickItem *>(QStringLiteral("thumbnailCard"));
    QVERIFY(card);
    auto *idleTimer = window->findChild<QObject *>(QStringLiteral("idleTimer"));
    QVERIFY(idleTimer);
    object->setProperty("hovered", false);
    const QPoint center(static_cast<int>(card->x() + card->width() / 2),
                        static_cast<int>(card->y() + card->height() / 2));

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, center);
    QTRY_VERIFY_WITH_TIMEOUT(object->property("fileDragArmed").toBool(), 1000);
    QCOMPARE(object->property("nativeDragStartCount").toInt(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(card->scale() > 1.04, 1000);

    QTest::mouseMove(window, center + QPoint(20, 0), 0);
    QTRY_COMPARE(object->property("nativeDragStartCount").toInt(), 1);
    QVERIFY(object->property("nativeFileDragging").toBool());
    QVERIFY(object->property("nativeDragConsumedPress").toBool());
    object->setProperty("hovered", false);
    QVERIFY(!idleTimer->property("running").toBool());
    QVERIFY(window->isVisible());
    QCOMPARE(fixture.inputRegion.region().boundingRect(), QRect(0, 0, 1, 1));

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
    QTRY_VERIFY_WITH_TIMEOUT(object->property("fileDragArmed").toBool(), 1000);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, center);
    QTRY_VERIFY_WITH_TIMEOUT(fixture.controller.previewVisible(), 1000);
    fixture.controller.closePreview();
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, center);
    QTRY_VERIFY_WITH_TIMEOUT(object->property("fileDragArmed").toBool(), 1000);
    QTest::mouseMove(window, center + QPoint(20, 0), 0);
    QTRY_COMPARE(object->property("nativeDragStartCount").toInt(), 2);
    object->setProperty("hovered", false);
    QVERIFY(!idleTimer->property("running").toBool());
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, center + QPoint(20, 0));
    QVariant copyAction;
    QVERIFY(QMetaObject::invokeMethod(object.get(), "finishNativeFileDrag",
                                      Q_RETURN_ARG(QVariant, copyAction),
                                      Q_ARG(QVariant, QVariant::fromValue(int(Qt::CopyAction)))));
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
        QString::fromUtf8("/tmp/Captura de tela/截图 final.png");
    std::unique_ptr<QObject> object(load(fixture.engine, QStringLiteral("ScreenshotThumbnail.qml")));

    const QVariantMap mimeData = object->property("nativeDragMimeData").toMap();
    QCOMPARE(mimeData.value(QStringLiteral("text/uri-list")).toString(),
             fixture.controller.resultUri());
    QCOMPARE(mimeData.value(QStringLiteral("text/plain")).toString(),
             fixture.controller.resultPath());
    QCOMPARE(object->property("nativeDragSupportedActions").toInt(),
             int(Qt::CopyAction));
    QCOMPARE(object->property("nativeDragProposedAction").toInt(),
             int(Qt::CopyAction));
    QVERIFY(object->property("nativeDragUsesManualStart").toBool());
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

    fixture.controller.m_thumbnailVisible = true;
    emit fixture.controller.thumbnailVisibleChanged();
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
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

    const QPoint restored(static_cast<int>(card->x() + card->width() / 2),
                          static_cast<int>(card->y() + card->height() / 2));
    const qreal slotX = object->property("slotPosition").toPointF().x();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, restored);
    QTest::mouseMove(window, restored + QPoint(20, 2), 0);
    QTRY_VERIFY_WITH_TIMEOUT(object->property("dragging").toBool(), 1000);
    QVERIFY(card->x() > slotX + 10.0);
    const qreal heldWidth = card->width() * card->scale();
    const qreal heldHeight = card->height() * card->scale();
    const QRectF heldVisualRect(card->x() - (heldWidth - card->width()) / 2,
                                card->y() - (heldHeight - card->height()) / 2,
                                heldWidth, heldHeight);
    QVERIFY(std::abs(fixture.controller.thumbnailRect().x() - heldVisualRect.x()) < 1.0);
    QVERIFY(std::abs(fixture.controller.thumbnailRect().y() - heldVisualRect.y()) < 1.0);
    QVERIFY(std::abs(fixture.controller.thumbnailRect().width() - heldVisualRect.width()) < 1.0);
    QVERIFY(std::abs(fixture.controller.thumbnailRect().height() - heldVisualRect.height()) < 1.0);
    QVERIFY(fixture.inputRegion.region().contains(
        QPoint(static_cast<int>(card->x() + card->width() / 2),
               static_cast<int>(card->y() + card->height() / 2))));
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, restored + QPoint(20, 2));
    QTRY_VERIFY_WITH_TIMEOUT(std::abs(card->x() - object->property("slotPosition").toPointF().x())
                                 < 1.0,
                             1000);
    QVERIFY(fixture.controller.thumbnailVisible());
    QVERIFY(!fixture.controller.previewVisible());

    const QPoint swipeStart(static_cast<int>(card->x() + card->width() / 2),
                            static_cast<int>(card->y() + card->height() / 2));
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, swipeStart);
    QTest::mouseMove(window, swipeStart + QPoint(110, 0), 0);
    QTRY_VERIFY_WITH_TIMEOUT(object->property("dragging").toBool(), 1000);
    QVERIFY(card->x() > slotX + 90.0);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, swipeStart + QPoint(110, 0));
    QTRY_VERIFY_WITH_TIMEOUT(!window->isVisible(), 1000);
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
    timer->setProperty("interval", 1);
    QTRY_VERIFY_WITH_TIMEOUT(!window->isVisible(), 1000);
}

void ScreenshotThumbnailQmlTest::previewStartsHiddenOpensExistingResultAndEscapes()
{
    ScreenshotQmlFixture fixture;
    fixture.controller.m_image = QImage(160, 100, QImage::Format_RGBA8888);
    fixture.controller.m_imageGeneration = 1;
    fixture.controller.m_resultPath = QStringLiteral("/tmp/astrea-screenshot-test.png");
    int captureCount = 0;
    fixture.controller.m_captureRequest = [&captureCount] {
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
    QVERIFY(backdrop);
    QVERIFY(card);
    QVERIFY(image);

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
    QVERIFY(targetRect.width() <= window->width() * 0.78 + 1.0);
    QVERIFY(targetRect.height() <= window->height() * 0.78 + 1.0);
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
