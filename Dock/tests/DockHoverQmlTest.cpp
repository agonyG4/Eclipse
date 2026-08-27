#include "core/DockController.hpp"
#include "core/DockSurfaceGeometry.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>

#include <functional>

namespace {

class FakeContextMenuController final : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE bool presentDock(const QString &desktopFileName, int x, int y, int width,
                                 int height, const QString &outputKey)
    {
        ++presentCount;
        lastDesktopFileName = desktopFileName;
        lastRectangle = QRect(x, y, width, height);
        lastOutputKey = outputKey;
        return true;
    }

    int presentCount = 0;
    QString lastDesktopFileName;
    QRect lastRectangle;
    QString lastOutputKey;
};

const QString kDockPanelPath = QString::fromUtf8(
    DOCK_SOURCE_DIR "/qml/components/DockPanel.qml");

QQuickItem *delegate(QQuickItem *panel, const QString &desktopFileName)
{
    QQuickItem *result = nullptr;
    std::function<void(QQuickItem *)> visit = [&result, &desktopFileName,
                                               &visit](QQuickItem *item) {
        if (result || !item)
            return;
        if (item->objectName() == desktopFileName) {
            result = item;
            return;
        }
        for (QQuickItem *child : item->childItems())
            visit(child);
    };
    visit(panel);
    return result;
}

QQuickItem *iconItem(QQuickItem *delegateItem)
{
    QQuickItem *result = nullptr;
    std::function<void(QQuickItem *)> visit = [&result, &visit](QQuickItem *item) {
        if (result || !item)
            return;
        if (item->property("sourcePixelSize").isValid()) {
            result = item;
            return;
        }
        for (QQuickItem *child : item->childItems())
            visit(child);
    };
    visit(delegateItem);
    return result;
}

QPoint itemCenter(QQuickWindow &window, QQuickItem *item)
{
    return item->mapToItem(window.contentItem(), item->width() / 2.0,
                           item->height() / 2.0)
        .toPoint();
}

double propertyReal(QQuickItem *item, const char *name)
{
    return item->property(name).toDouble();
}

DockConfig configFor(const QString &hoverEffect, const QStringList &pins)
{
    DockConfig config = DockConfig::defaults();
    config.hoverEffect = hoverEffect;
    config.pins = pins;
    return config;
}

} // namespace

class DockHoverQmlTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void visualHeadroomKeepsIconsInBoundsForEveryIconSize();
    void hoverModesAndTransitions();
    void reorderPreviewWorksForEveryHoverMode();
    void pointerHandlersActivateAndReorderExactlyOnce();
    void pointerHandlerCancellationRestoresWithoutReorder();
    void magnifiedVisualRegionAcceptsContextMenu();
};

void DockHoverQmlTest::initTestCase()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
}

void DockHoverQmlTest::visualHeadroomKeepsIconsInBoundsForEveryIconSize()
{
    for (int iconSize = 32; iconSize <= 64; ++iconSize) {
        DockController controller;
        const QStringList pins{QStringLiteral("one.desktop")};
        DockConfig config = configFor(QStringLiteral("lift"), pins);
        config.iconSize = iconSize;
        controller.applyConfig(config);

        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
        engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
        QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
        QVERIFY2(component.status() == QQmlComponent::Ready,
                 qPrintable(component.errorString()));
        auto *panel = qobject_cast<QQuickItem *>(component.create());
        QVERIFY2(panel, qPrintable(component.errorString()));
        QQuickWindow window;
        panel->setParentItem(window.contentItem());
        QCoreApplication::processEvents();

        QQuickItem *delegateItem = delegate(panel, QStringLiteral("one.desktop"));
        QQuickItem *icon = iconItem(delegateItem);
        QVERIFY(delegateItem && icon);
        const double expectedRestingTop = delegateItem->mapToItem(panel, 0, 0).y()
            + (delegateItem->height() - iconSize) / 2.0;
        const double actualRestingTop = icon->mapToItem(panel, 0, 0).y();
        QVERIFY2(qAbs(actualRestingTop - expectedRestingTop) < 0.5,
                 qPrintable(QStringLiteral("iconSize=%1 resting baseline=%2 expected=%3")
                                .arg(iconSize).arg(actualRestingTop).arg(expectedRestingTop)));
        QQuickItem *chrome = delegate(panel, QStringLiteral("dockChrome"));
        QVERIFY(chrome);
        QCOMPARE(chrome->height(), controller.restingHeight());

        QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                          Q_ARG(QVariant, QVariant(panel->width() / 2.0))));
        QTRY_VERIFY_WITH_TIMEOUT(panel->property("height").toDouble()
                                     >= controller.restingHeight(),
                                 1000);
        QTRY_VERIFY_WITH_TIMEOUT(icon->mapToItem(panel, 0, 0).y() >= -0.01, 1000);
        QCOMPARE(chrome->height(), controller.restingHeight());

        config.hoverEffect = QStringLiteral("magnification");
        controller.applyConfig(config);
        QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                          Q_ARG(QVariant, QVariant(panel->width() / 2.0))));
        QTRY_VERIFY_WITH_TIMEOUT(icon->mapToItem(panel, 0, 0).y() >= -0.01, 1000);
        QCOMPARE(chrome->height(), controller.restingHeight());

        config.hoverEffect = QStringLiteral("none");
        controller.applyConfig(config);
        QVERIFY(QMetaObject::invokeMethod(panel, "beginReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop")))));
        QVERIFY(QMetaObject::invokeMethod(panel, "updateReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop"))),
                                          Q_ARG(QVariant, QVariant(0.0))));
        QTRY_VERIFY_WITH_TIMEOUT(icon->mapToItem(panel, 0, 0).y() >= -0.01, 1000);
        QCOMPARE(chrome->height(), controller.restingHeight());
        QVERIFY(QMetaObject::invokeMethod(panel, "cancelReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop")))));
        delete panel;
    }
}

void DockHoverQmlTest::pointerHandlersActivateAndReorderExactlyOnce()
{
    DockController controller;
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    controller.applyConfig(configFor(QStringLiteral("none"), pins));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(400, 120);
    window.show();
    QTest::qWait(20);

    QQuickItem *first = delegate(panel, QStringLiteral("one.desktop"));
    QVERIFY(first);
    QSignalSpy activationSpy(first, SIGNAL(activated(QString)));
    QSignalSpy finishSpy(first, SIGNAL(dragFinished(QString)));
    QSignalSpy cancelSpy(first, SIGNAL(dragCanceled(QString)));
    QSignalSpy reorderSpy(panel, SIGNAL(reorderRequested(QString,int)));
    QVERIFY(activationSpy.isValid() && finishSpy.isValid() && cancelSpy.isValid()
            && reorderSpy.isValid());

    const QPoint start = itemCenter(window, first);
    QTest::mouseMove(&window, start);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTRY_COMPARE_WITH_TIMEOUT(activationSpy.count(), 1, 1000);
    QCOMPARE(finishSpy.count(), 0);
    QCOMPARE(cancelSpy.count(), 0);

    activationSpy.clear();
    const QPoint dragEnd = start + QPoint(140, 0);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&window, start + QPoint(20, 0), 30);
    QTest::mouseMove(&window, dragEnd, 30);
    QCoreApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(first->property("dragging").toBool(), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("dragTargetIndex").toInt(), 2, 1000);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, dragEnd);
    QTRY_COMPARE_WITH_TIMEOUT(reorderSpy.count(), 1, 1000);
    QCOMPARE(activationSpy.count(), 0);
    QCOMPARE(finishSpy.count(), 1);
    QCOMPARE(cancelSpy.count(), 0);

    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTRY_COMPARE_WITH_TIMEOUT(activationSpy.count(), 1, 1000);
    QCOMPARE(finishSpy.count(), 1);
    QCOMPARE(cancelSpy.count(), 0);

    delete panel;
}

void DockHoverQmlTest::pointerHandlerCancellationRestoresWithoutReorder()
{
    DockController controller;
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    controller.applyConfig(configFor(QStringLiteral("none"), pins));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(400, 120);
    window.show();
    QTest::qWait(20);

    QQuickItem *first = delegate(panel, QStringLiteral("one.desktop"));
    QVERIFY(first);
    QSignalSpy activationSpy(first, SIGNAL(activated(QString)));
    QSignalSpy finishSpy(first, SIGNAL(dragFinished(QString)));
    QSignalSpy cancelSpy(first, SIGNAL(dragCanceled(QString)));
    QSignalSpy reorderSpy(panel, SIGNAL(reorderRequested(QString,int)));
    QVERIFY(activationSpy.isValid() && finishSpy.isValid() && cancelSpy.isValid()
            && reorderSpy.isValid());

    const QPoint start = itemCenter(window, first);
    const QPoint dragEnd = start + QPoint(140, 0);
    QTest::mouseMove(&window, start);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&window, start + QPoint(20, 0), 30);
    QTest::mouseMove(&window, dragEnd, 30);
    QCoreApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(first->property("dragging").toBool(), 1000);
    QVERIFY(QMetaObject::invokeMethod(panel, "cancelReorder",
                                      Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop")))));
    QVERIFY(!first->property("dragging").toBool());
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, dragEnd);
    QCoreApplication::processEvents();
    QCOMPARE(reorderSpy.count(), 0);
    QCOMPARE(activationSpy.count(), 0);

    window.show();
    QTest::qWait(20);
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTRY_COMPARE_WITH_TIMEOUT(activationSpy.count(), 1, 1000);
    QCOMPARE(reorderSpy.count(), 0);
    QCOMPARE(cancelSpy.count(), 0);

    delete panel;
}

void DockHoverQmlTest::magnifiedVisualRegionAcceptsContextMenu()
{
    DockController dockController;
    dockController.applyConfig(configFor(QStringLiteral("magnification"),
                                         {QStringLiteral("one.desktop")}));
    FakeContextMenuController contextMenuController;
    DockSurfaceGeometry surfaceGeometry;

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &dockController);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    panel->setProperty("contextMenuController", QVariant::fromValue(
                                                    static_cast<QObject *>(&contextMenuController)));
    panel->setProperty("dockSurfaceGeometry", QVariant::fromValue(
                                                  static_cast<QObject *>(&surfaceGeometry)));
    panel->setProperty("outputKey", QStringLiteral("output-1"));
    panel->setProperty("outputWidth", 400);
    panel->setProperty("outputHeight", 120);

    QQuickWindow window;
    window.resize(400, 120);
    panel->setParentItem(window.contentItem());
    const auto centerPanel = [panel, &window]() {
        panel->setX((window.width() - panel->width()) / 2.0);
    };
    QObject::connect(panel, &QQuickItem::widthChanged, panel, centerPanel);
    centerPanel();
    window.show();
    QTest::qWait(20);

    QQuickItem *delegateItem = delegate(panel, QStringLiteral("one.desktop"));
    QQuickItem *icon = iconItem(delegateItem);
    QVERIFY(delegateItem && icon);
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(panel->width() / 2.0))));
    const QRectF restingRect = delegateItem->mapRectToItem(window.contentItem(),
                                                            QRectF(0, 0, delegateItem->width(),
                                                                   delegateItem->height()));
    QTRY_VERIFY_WITH_TIMEOUT(
        icon->scale() > 1.0
            && icon->mapRectToItem(window.contentItem(),
                                   QRectF(0, 0, icon->width(), icon->height())).right()
                > restingRect.right(),
        2000);
    const QRectF visualRect = icon->mapRectToItem(window.contentItem(),
                                                   QRectF(0, 0, icon->width(), icon->height()));
    QVERIFY(visualRect.right() > restingRect.right());
    const QPoint outsideRestingInsideVisual(
        qRound(visualRect.right() - 0.5), qRound(visualRect.center().y()));
    QVERIFY(visualRect.contains(outsideRestingInsideVisual));
    QVERIFY2(!restingRect.contains(outsideRestingInsideVisual),
             qPrintable(QStringLiteral("visual=%1,%2 %3x%4 resting=%5,%6 %7x%8 point=%9,%10")
                            .arg(visualRect.x()).arg(visualRect.y())
                            .arg(visualRect.width()).arg(visualRect.height())
                            .arg(restingRect.x()).arg(restingRect.y())
                            .arg(restingRect.width()).arg(restingRect.height())
                            .arg(outsideRestingInsideVisual.x())
                            .arg(outsideRestingInsideVisual.y())));

    QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier, outsideRestingInsideVisual);
    QTRY_COMPARE_WITH_TIMEOUT(contextMenuController.presentCount, 1, 1000);
    QCOMPARE(contextMenuController.lastDesktopFileName, QStringLiteral("one.desktop"));
    QCOMPARE(contextMenuController.lastOutputKey, QStringLiteral("output-1"));
    QVERIFY(contextMenuController.lastRectangle.width() > 0);
    QVERIFY(contextMenuController.lastRectangle.height() > 0);

    delete panel;
}

void DockHoverQmlTest::hoverModesAndTransitions()
{
    DockController controller;
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop"), QStringLiteral("four.desktop"),
        QStringLiteral("five.desktop"), QStringLiteral("six.desktop"),
        QStringLiteral("seven.desktop")};
    controller.applyConfig(configFor(QStringLiteral("magnification"), pins));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    QCoreApplication::processEvents();
    QVERIFY(panel->width() > 0);

    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(panel->width() / 2.0))));

    QQuickItem *farLeft = delegate(panel, QStringLiteral("one.desktop"));
    QQuickItem *nearLeft = delegate(panel, QStringLiteral("three.desktop"));
    QQuickItem *center = delegate(panel, QStringLiteral("four.desktop"));
    QQuickItem *nearRight = delegate(panel, QStringLiteral("five.desktop"));
    QQuickItem *farRight = delegate(panel, QStringLiteral("seven.desktop"));
    QVERIFY(farLeft && nearLeft && center && nearRight && farRight);
    QTRY_VERIFY_WITH_TIMEOUT(propertyReal(center, "magnificationScale")
                                 > propertyReal(nearLeft, "magnificationScale"),
                             1000);
    QCOMPARE(propertyReal(nearLeft, "magnificationScale"),
             propertyReal(nearRight, "magnificationScale"));
    QCOMPARE(propertyReal(farLeft, "magnificationScale"),
             propertyReal(farRight, "magnificationScale"));
    QCOMPARE(propertyReal(farLeft, "magnificationScale"), 1.0);
    QVERIFY(propertyReal(nearLeft, "visualOffsetX") < 0.0);
    QVERIFY(propertyReal(nearRight, "visualOffsetX") > 0.0);

    const double rowWidth = pins.size() * controller.delegateWidth()
        + (pins.size() - 1) * controller.itemSpacing();
    const double boundaryX = panel->width() / 2.0
        - rowWidth / 2.0 + controller.delegateWidth() / 2.0
        + 3.5 * controller.itemSpacing() + 3.5 * controller.delegateWidth();
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(boundaryX))));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(propertyReal(center, "magnificationScale")
                                      - propertyReal(nearRight, "magnificationScale"))
                                 < 0.005,
                             1000);

    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(panel->width() / 2.0))));
    QTRY_VERIFY_WITH_TIMEOUT(propertyReal(center, "magnificationScale")
                                 > propertyReal(nearLeft, "magnificationScale"),
                             1000);

    const double restingWidth = panel->property("restingWidth").toDouble();
    const double restingHeight = panel->property("restingHeight").toDouble();
    QVERIFY(panel->height() > restingHeight);
    controller.applyConfig(configFor(QStringLiteral("lift"), pins));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->width() - restingWidth) < 0.1, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->height() - restingHeight) < 0.1, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(propertyReal(center, "magnificationScale"), 1.1, 1000);
    QCOMPARE(propertyReal(farLeft, "magnificationScale"), 1.0);
    QCOMPARE(propertyReal(farRight, "magnificationScale"), 1.0);
    QCOMPARE(propertyReal(center, "visualOffsetY"), -5.0);
    QCOMPARE(propertyReal(farLeft, "visualOffsetY"), 0.0);
    QCOMPARE(propertyReal(farRight, "visualOffsetY"), 0.0);
    QCOMPARE(propertyReal(farLeft, "visualOffsetX"), 0.0);
    QCOMPARE(propertyReal(farRight, "visualOffsetX"), 0.0);

    controller.applyConfig(configFor(QStringLiteral("none"), pins));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->width() - restingWidth) < 0.1, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->height() - restingHeight) < 0.1, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(propertyReal(center, "magnificationScale"), 1.0, 1000);
    QCOMPARE(propertyReal(center, "visualOffsetY"), 0.0);
    QCOMPARE(propertyReal(center, "visualOffsetX"), 0.0);
    QCOMPARE(propertyReal(farLeft, "magnificationScale"), 1.0);
    QCOMPARE(propertyReal(farRight, "magnificationScale"), 1.0);

    QVERIFY(QMetaObject::invokeMethod(panel, "setPointerInside",
                                      Q_ARG(QVariant, QVariant(false))));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->width() - restingWidth) < 0.1, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->height() - restingHeight) < 0.1, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(propertyReal(center, "magnificationScale"), 1.0, 1000);
    QCOMPARE(propertyReal(center, "visualOffsetX"), 0.0);
    QCOMPARE(propertyReal(center, "visualOffsetY"), 0.0);

    delete panel;
}

void DockHoverQmlTest::reorderPreviewWorksForEveryHoverMode()
{
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    for (const QString &effect : {QStringLiteral("none"), QStringLiteral("lift"),
                                  QStringLiteral("magnification")}) {
        DockController controller;
        controller.applyConfig(configFor(effect, pins));

        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
        engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
        QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
        QVERIFY2(component.status() == QQmlComponent::Ready,
                 qPrintable(component.errorString()));
        auto *panel = qobject_cast<QQuickItem *>(component.create());
        QVERIFY2(panel, qPrintable(component.errorString()));
        QQuickWindow window;
        panel->setParentItem(window.contentItem());
        QCoreApplication::processEvents();

        QSignalSpy reorderSpy(panel, SIGNAL(reorderRequested(QString,int)));
        QVERIFY(QMetaObject::invokeMethod(panel, "beginReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop")))));
        QVERIFY(QMetaObject::invokeMethod(panel, "updateReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop"))),
                                          Q_ARG(QVariant, QVariant(140.0))));
        QQuickItem *dragged = delegate(panel, QStringLiteral("one.desktop"));
        QQuickItem *neighbor = delegate(panel, QStringLiteral("two.desktop"));
        QVERIFY(dragged && neighbor);
        QVERIFY(dragged->property("dragging").toBool());
        QSignalSpy activationSpy(dragged, SIGNAL(activated(QString)));
        QVERIFY(activationSpy.isValid());
        QTRY_VERIFY_WITH_TIMEOUT(propertyReal(neighbor, "visualOffsetX") < 0.0, 1000);
        if (effect == QStringLiteral("magnification"))
            QCOMPARE(propertyReal(dragged, "magnificationScale"), 1.0);

        QVERIFY(QMetaObject::invokeMethod(panel, "finishReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop")))));
        QCOMPARE(reorderSpy.count(), 1);
        QCOMPARE(reorderSpy.at(0).at(0).toString(), QStringLiteral("one.desktop"));
        QCOMPARE(reorderSpy.at(0).at(1).toInt(), 2);
        QCOMPARE(activationSpy.count(), 0);
        delete panel;
    }
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication application(argc, argv);
    DockHoverQmlTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "DockHoverQmlTest.moc"
