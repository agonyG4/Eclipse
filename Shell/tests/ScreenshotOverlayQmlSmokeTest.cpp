#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest/QtTest>

#include "platform/typhon/TyphonScreenCaptureClient.hpp"
#include "platform/typhon/TyphonSharedConnection.hpp"
#include "screenshot/ScreenshotController.hpp"
#include "screenshot/ScreenshotImageProvider.hpp"

class ScreenshotOverlayQmlSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void overlayCreatesHiddenAndSupportsFrozenAndLiveModes();
    void selectionDragStaysLocalUntilNormalizedRelease();
};

void ScreenshotOverlayQmlSmokeTest::overlayCreatesHiddenAndSupportsFrozenAndLiveModes()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    controller.m_image = QImage(4, 4, QImage::Format_RGBA8888);
    controller.m_imageGeneration = 1;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("ScreenshotController"), &controller);
    engine.addImageProvider(QStringLiteral("astrea-screenshot"),
                             new ScreenshotImageProvider(&controller));

    QQmlComponent component(
        &engine,
        QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Screenshot/ScreenshotOverlay.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    QObject *object = component.create();
    QVERIFY2(object, qPrintable(component.errorString()));
    QVERIFY2(component.errors().isEmpty(), qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(object);
    QVERIFY(window);
    QVERIFY(!window->isVisible());

    auto *keyboardScope = window->findChild<QQuickItem *>(QStringLiteral("keyboardScope"));
    QVERIFY(keyboardScope);
    auto *selectionImage = window->findChild<QQuickItem *>(QStringLiteral("selectionImage"));
    QVERIFY(selectionImage);

    controller.m_intent = ScreenshotIntent::FrozenRegion;
    controller.m_phase = ScreenshotPhase::FrozenSelecting;
    controller.m_visible = true;
    controller.m_selectionImageVisible = true;
    emit controller.visibleChanged();
    emit controller.selectionImageVisibleChanged();
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
    QVERIFY(selectionImage->isVisible());

    keyboardScope->forceActiveFocus();
    QTRY_VERIFY_WITH_TIMEOUT(keyboardScope->hasActiveFocus(), 1000);
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isVisible(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!window->isVisible(), 1000);

    controller.m_intent = ScreenshotIntent::LiveRegion;
    controller.m_phase = ScreenshotPhase::LiveSelecting;
    controller.m_visible = true;
    controller.m_selectionImageVisible = false;
    emit controller.visibleChanged();
    emit controller.selectionImageVisibleChanged();
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
    QVERIFY(!selectionImage->isVisible());
    QCOMPARE(window->color(), QColor(Qt::transparent));

    QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, QPoint(20, 20));
    QTRY_VERIFY_WITH_TIMEOUT(!window->isVisible(), 1000);
    QVERIFY(!controller.isVisible());

    delete window;
}

void ScreenshotOverlayQmlSmokeTest::selectionDragStaysLocalUntilNormalizedRelease()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    controller.m_image = QImage(4, 4, QImage::Format_RGBA8888);
    controller.m_imageGeneration = 1;
    int captureCount = 0;
    controller.m_captureRequest = [&captureCount] {
        ++captureCount;
        return true;
    };

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("ScreenshotController"), &controller);
    engine.addImageProvider(QStringLiteral("astrea-screenshot"),
                             new ScreenshotImageProvider(&controller));

    QQmlComponent component(
        &engine,
        QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Screenshot/ScreenshotOverlay.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(object.get());
    QVERIFY(window);
    window->resize(640, 480);

    controller.m_intent = ScreenshotIntent::LiveRegion;
    controller.m_phase = ScreenshotPhase::LiveSelecting;
    controller.m_visible = true;
    emit controller.intentChanged();
    emit controller.phaseChanged();
    emit controller.visibleChanged();
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);

    auto *fullDim = window->findChild<QQuickItem *>(QStringLiteral("fullDim"));
    auto *topDim = window->findChild<QQuickItem *>(QStringLiteral("topDim"));
    auto *bottomDim = window->findChild<QQuickItem *>(QStringLiteral("bottomDim"));
    auto *leftDim = window->findChild<QQuickItem *>(QStringLiteral("leftDim"));
    auto *rightDim = window->findChild<QQuickItem *>(QStringLiteral("rightDim"));
    auto *border = window->findChild<QQuickItem *>(QStringLiteral("selectionBorder"));
    auto *mouseArea = window->findChild<QQuickItem *>(QStringLiteral("selectionMouseArea"));
    auto *selectionImage = window->findChild<QQuickItem *>(QStringLiteral("selectionImage"));
    QVERIFY(fullDim);
    QVERIFY(topDim);
    QVERIFY(bottomDim);
    QVERIFY(leftDim);
    QVERIFY(rightDim);
    QVERIFY(border);
    QVERIFY(mouseArea);
    QVERIFY(selectionImage);
    QVERIFY(selectionImage->property("smooth").isValid());
    QVERIFY(!selectionImage->property("smooth").toBool());
    QVERIFY(fullDim->isVisible());
    QVERIFY(!topDim->isVisible());
    QVERIFY(!bottomDim->isVisible());
    QVERIFY(!leftDim->isVisible());
    QVERIFY(!rightDim->isVisible());
    QVERIFY(!border->isVisible());
    QCOMPARE(fullDim->property("color").value<QColor>(), QColor(QStringLiteral("#66000000")));
    QCOMPARE(topDim->property("color").value<QColor>(), QColor(QStringLiteral("#66000000")));
    QCOMPARE(bottomDim->property("color").value<QColor>(), QColor(QStringLiteral("#66000000")));
    QCOMPARE(leftDim->property("color").value<QColor>(), QColor(QStringLiteral("#66000000")));
    QCOMPARE(rightDim->property("color").value<QColor>(), QColor(QStringLiteral("#66000000")));
    QCOMPARE(border->property("selectionBorderColor").value<QColor>(),
             QColor(QStringLiteral("#f5f5f5")));
    QCOMPARE(border->property("selectionBorderWidth").toReal(), 1.0);

    const QPoint forwardStart(100, 100);
    const QPoint forwardEnd(300, 250);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, forwardStart);
    QTest::mouseMove(window, forwardEnd, 0);
    QTRY_VERIFY_WITH_TIMEOUT(object->property("selecting").toBool(), 1000);
    QVERIFY(object->property("selectionWidth").toReal() > 199.0);
    QVERIFY(object->property("selectionHeight").toReal() > 149.0);
    QVERIFY(controller.selection().isEmpty());
    QVERIFY(!fullDim->isVisible());
    QVERIFY(topDim->isVisible());
    QVERIFY(bottomDim->isVisible());
    QVERIFY(leftDim->isVisible());
    QVERIFY(rightDim->isVisible());
    QCOMPARE(topDim->width(), window->width());
    QCOMPARE(bottomDim->width(), window->width());
    QCOMPARE(leftDim->x(), 0.0);
    QCOMPARE(rightDim->x(), 300.0);
    QVERIFY(topDim->height() > 99.0);
    QVERIFY(bottomDim->height() > 229.0);
    QVERIFY(border->isVisible());
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, forwardEnd);
    QTRY_VERIFY_WITH_TIMEOUT(controller.phase() == ScreenshotPhase::WaitingCapture, 1000);
    QCOMPARE(captureCount, 1);
    QCOMPARE(controller.selection(), QRectF(100, 100, 200, 150));
    QCOMPARE(object->property("selectionLeft").toReal(), 100.0);
    QCOMPARE(object->property("selectionTop").toReal(), 100.0);
    QVERIFY(!fullDim->property("visible").toBool());
    QVERIFY(topDim->property("visible").toBool());
    QVERIFY(bottomDim->property("visible").toBool());
    QVERIFY(leftDim->property("visible").toBool());
    QVERIFY(rightDim->property("visible").toBool());

    controller.m_phase = ScreenshotPhase::LiveSelecting;
    controller.m_selection = {};
    controller.m_visible = true;
    emit controller.phaseChanged();
    emit controller.selectionChanged();
    emit controller.visibleChanged();
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
    QCOMPARE(object->property("startX").toReal(), 0.0);
    QCOMPARE(object->property("currentX").toReal(), 0.0);

    const QPoint reverseStart(300, 250);
    const QPoint reverseEnd(100, 100);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, reverseStart);
    QTest::mouseMove(window, reverseEnd, 0);
    QVERIFY(controller.selection().isEmpty());
    QCOMPARE(object->property("selectionLeft").toReal(), 100.0);
    QCOMPARE(object->property("selectionTop").toReal(), 100.0);
    QCOMPARE(object->property("selectionWidth").toReal(), 200.0);
    QCOMPARE(object->property("selectionHeight").toReal(), 150.0);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, reverseEnd);
    QTRY_VERIFY_WITH_TIMEOUT(controller.phase() == ScreenshotPhase::WaitingCapture, 1000);
    QCOMPARE(captureCount, 2);
    QCOMPARE(controller.selection(), QRectF(100, 100, 200, 150));

    controller.cancel();
}

QTEST_MAIN(ScreenshotOverlayQmlSmokeTest)
#include "ScreenshotOverlayQmlSmokeTest.moc"
