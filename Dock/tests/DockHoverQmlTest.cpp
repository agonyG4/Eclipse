#include "core/DockController.hpp"
#include "core/DockSurfaceGeometry.hpp"
#include "services/DockConfigPersistence.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointingDevice>
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

class QmlCountingPersistence final : public DockConfigPersistence {
public:
    QmlCountingPersistence() : DockConfigPersistence(QStringLiteral("/unused/dock.json")) {}

    bool writePins(const QStringList &pins, QString *errorOut = nullptr) override
    {
        ++calls;
        lastPins = pins;
        if (errorOut)
            errorOut->clear();
        return true;
    }

    int calls = 0;
    QStringList lastPins;
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

QQuickItem *childWithObjectName(QQuickItem *parent, const QString &objectName)
{
    QQuickItem *result = nullptr;
    std::function<void(QQuickItem *)> visit = [&result, &objectName,
                                               &visit](QQuickItem *item) {
        if (result || !item)
            return;
        if (item->objectName() == objectName) {
            result = item;
            return;
        }
        for (QQuickItem *child : item->childItems())
            visit(child);
    };
    visit(parent);
    return result;
}

QPoint itemCenter(QQuickWindow &window, QQuickItem *item)
{
    return item->mapToItem(window.contentItem(), item->width() / 2.0,
                           item->height() / 2.0)
        .toPoint();
}

QPointF visualCenter(QQuickWindow &window, QQuickItem *item)
{
    return item->mapToItem(window.contentItem(), item->width() / 2.0,
                           item->height() / 2.0);
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
    void pointerHandlerGrabTransitionsFinalizeExactlyOnce();
    void dragGeometryRemainsStableDuringMagnificationCollapse();
    void modelMoveRefreshesHoverGeometryAndIdentity();
    void releasePointerRestoresMagnificationTarget();
    void reorderLiftsOnlyTheDraggedDelegate();
    void magnifiedVisualRegionAcceptsContextMenu();
    void magnifiedInteractionTargetMatchesVisualBounds();
    void magnifiedInteractionRegionsResolveByVisualStacking();
    void exclusiveDragReleaseInEmptyHeadroomStaysOutsideDock();
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
                                          Q_ARG(QVariant, QVariant(0.0)),
                                          Q_ARG(QVariant, QVariant(0.0)),
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

    QTest::mouseMove(&window, start);
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
    QTest::mouseMove(&window, start);
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTRY_COMPARE_WITH_TIMEOUT(activationSpy.count(), 1, 1000);
    QCOMPARE(reorderSpy.count(), 0);
    QCOMPARE(cancelSpy.count(), 0);

    delete panel;
}

void DockHoverQmlTest::pointerHandlerGrabTransitionsFinalizeExactlyOnce()
{
    DockController controller;
    controller.applyConfig(configFor(QStringLiteral("none"),
                                     {QStringLiteral("one.desktop")}));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));

    QQuickItem *item = delegate(panel, QStringLiteral("one.desktop"));
    QVERIFY(item);
    QSignalSpy startedSpy(item, SIGNAL(dragStarted(QString)));
    QSignalSpy finishSpy(item, SIGNAL(dragFinished(QString)));
    QSignalSpy cancelSpy(item, SIGNAL(dragCanceled(QString)));
    QSignalSpy reorderSpy(panel, SIGNAL(reorderRequested(QString,int)));
    QVERIFY(startedSpy.isValid() && finishSpy.isValid() && cancelSpy.isValid()
            && reorderSpy.isValid());

    const auto transition = [](QPointingDevice::GrabTransition value) {
        return QVariant::fromValue(static_cast<int>(value));
    };
    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::GrabPassive))));
    QCOMPARE(startedSpy.count(), 0);
    QCOMPARE(finishSpy.count(), 0);
    QCOMPARE(cancelSpy.count(), 0);

    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::GrabExclusive))));
    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(item->property("dragging").toBool());
    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::UngrabPassive))));
    QCOMPARE(finishSpy.count(), 0);
    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::UngrabExclusive))));
    QCOMPARE(finishSpy.count(), 1);
    QCOMPARE(cancelSpy.count(), 0);
    QCOMPARE(reorderSpy.count(), 0);
    QVERIFY(!item->property("dragging").toBool());
    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::UngrabExclusive))));
    QCOMPARE(finishSpy.count(), 1);

    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::GrabExclusive))));
    QCOMPARE(startedSpy.count(), 2);
    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::CancelGrabExclusive))));
    QCOMPARE(finishSpy.count(), 1);
    QCOMPARE(cancelSpy.count(), 1);
    QCOMPARE(reorderSpy.count(), 0);
    QVERIFY(!item->property("dragging").toBool());
    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::CancelGrabExclusive))));
    QCOMPARE(cancelSpy.count(), 1);

    delete panel;
}

void DockHoverQmlTest::dragGeometryRemainsStableDuringMagnificationCollapse()
{
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    for (int sourceIndex = 0; sourceIndex < pins.size(); ++sourceIndex) {
        DockController controller;
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
        panel->setY(20);
        window.resize(600, 180);
        const auto centerPanel = [panel]() {
            if (QQuickWindow *window = panel->window())
                panel->setX((window->width() - panel->width()) / 2.0);
        };
        QObject::connect(panel, &QQuickItem::widthChanged, panel, centerPanel);
        centerPanel();
        window.show();
        QTest::qWait(20);

        QQuickItem *source = delegate(panel, pins.at(sourceIndex));
        QQuickItem *icon = iconItem(source);
        QVERIFY(source && icon);
        const qreal sourceSlotCenter = source->mapToItem(
            panel, source->width() / 2.0, source->height() / 2.0).x();
        QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                          Q_ARG(QVariant, QVariant(sourceSlotCenter))));
        const double restingWidth = panel->property("restingWidth").toDouble();
        QTRY_VERIFY_WITH_TIMEOUT(panel->width() > restingWidth, 1000);

        QTRY_VERIFY_WITH_TIMEOUT(propertyReal(source, "magnificationScale") > 1.1, 1000);
        if (sourceIndex == 0 || sourceIndex == pins.size() - 1) {
            QTRY_VERIFY_WITH_TIMEOUT(qAbs(propertyReal(source, "visualOffsetX")) > 1.0,
                                     1000);
        }
        QTest::qWait(180);
        const QPoint start = visualCenter(window, icon).toPoint();
        const qreal initialCenterX = visualCenter(window, icon).x();
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(&window, start + QPoint(10, 0), 30);
        QTRY_VERIFY_WITH_TIMEOUT(source->property("dragging").toBool(), 1000);
        const qreal centerAfterGrab = visualCenter(window, icon).x();
        const int targetDuringCollapse = panel->property("dragTargetIndex").toInt();
        QVERIFY2(qAbs(centerAfterGrab - initialCenterX) < 1.0,
                 qPrintable(QStringLiteral("source index=%1 initial=%2 after grab=%3")
                                .arg(sourceIndex).arg(initialCenterX).arg(centerAfterGrab)));

        QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->width() - restingWidth) < 0.1, 1000);
        const qreal centerAfterCollapse = visualCenter(window, icon).x();
        QCOMPARE(panel->property("dragTargetIndex").toInt(), targetDuringCollapse);
        QVERIFY2(qAbs(centerAfterCollapse - centerAfterGrab) < 0.5,
                 qPrintable(QStringLiteral("source index=%1 grab=%2 after collapse=%3")
                                .arg(sourceIndex).arg(centerAfterGrab).arg(centerAfterCollapse)));

        const int direction = sourceIndex == pins.size() - 1 ? -120 : 120;
        const int expectedTarget = sourceIndex == pins.size() - 1 ? 0 : 2;
        const QPoint dragEnd = start + QPoint(direction, 0);
        QTest::mouseMove(&window, dragEnd, 30);
        QTRY_COMPARE_WITH_TIMEOUT(panel->property("dragTargetIndex").toInt(), expectedTarget,
                                  1000);
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, dragEnd);
        QTRY_VERIFY_WITH_TIMEOUT(!source->property("dragging").toBool(), 1000);
        delete panel;
    }
}

void DockHoverQmlTest::modelMoveRefreshesHoverGeometryAndIdentity()
{
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    QmlCountingPersistence persistence;
    DockController controller(nullptr, nullptr, &persistence);
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
    window.resize(600, 180);
    const auto centerPanel = [panel]() {
        if (QQuickWindow *window = panel->window())
            panel->setX((window->width() - panel->width()) / 2.0);
    };
    QObject::connect(panel, &QQuickItem::widthChanged, panel, centerPanel);
    centerPanel();
    window.show();
    QTest::qWait(20);

    QQuickItem *first = delegate(panel, QStringLiteral("one.desktop"));
    QVERIFY(first);
    const qreal firstSlotCenter = first->mapToItem(
        panel, first->width() / 2.0, first->height() / 2.0).x();
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(firstSlotCenter))));
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("pointerTargetDesktopFileName").toString(),
                              QStringLiteral("one.desktop"), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(propertyReal(first, "magnificationScale") > 1.1, 2000);

    QSignalSpy rowsMovedSpy(controller.appModel(),
                            SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)));
    QVERIFY(rowsMovedSpy.isValid());
    QVERIFY(controller.movePinned(QStringLiteral("one.desktop"), 2));
    QCOMPARE(persistence.calls, 1);
    QTRY_VERIFY_WITH_TIMEOUT(rowsMovedSpy.count() > 0, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("pointerTargetDesktopFileName").toString(),
                              QStringLiteral("two.desktop"), 1000);

    QQuickItem *newFirst = delegate(panel, QStringLiteral("two.desktop"));
    QQuickItem *newLast = delegate(panel, QStringLiteral("one.desktop"));
    QVERIFY(newFirst && newLast);
    QTRY_VERIFY_WITH_TIMEOUT(propertyReal(newFirst, "magnificationScale")
                                 > propertyReal(newLast, "magnificationScale"),
                             1000);
    QVERIFY(!panel->property("delegateKeys").isValid());

    QVERIFY(QMetaObject::invokeMethod(panel, "beginReorder",
                                      Q_ARG(QVariant, QVariant(QStringLiteral("two.desktop")))));
    QCOMPARE(panel->property("draggedDesktopFileName").toString(),
             QStringLiteral("two.desktop"));
    QCOMPARE(panel->property("draggedSourceIndex").toInt(), 0);
    QVERIFY(QMetaObject::invokeMethod(panel, "cancelReorder",
                                      Q_ARG(QVariant, QVariant(QStringLiteral("two.desktop")))));

    delete panel;
}

void DockHoverQmlTest::releasePointerRestoresMagnificationTarget()
{
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    QmlCountingPersistence persistence;
    DockController controller(nullptr, nullptr, &persistence);
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
    window.resize(600, 180);
    const auto centerPanel = [panel]() {
        if (QQuickWindow *window = panel->window())
            panel->setX((window->width() - panel->width()) / 2.0);
    };
    QObject::connect(panel, &QQuickItem::widthChanged, panel, centerPanel);
    centerPanel();
    window.show();
    QTest::qWait(20);

    QQuickItem *source = delegate(panel, QStringLiteral("one.desktop"));
    QQuickItem *icon = iconItem(source);
    QVERIFY(source && icon);
    const qreal firstSlotCenter = source->mapToItem(
        panel, source->width() / 2.0, source->height() / 2.0).x();
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(firstSlotCenter))));
    QTRY_VERIFY_WITH_TIMEOUT(propertyReal(source, "magnificationScale") > 1.1, 2000);
    const QPoint start = visualCenter(window, icon).toPoint();
    QTest::mouseMove(&window, start);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&window, start + QPoint(20, 0), 30);
    QTRY_VERIFY_WITH_TIMEOUT(source->property("dragging").toBool(), 1000);

    const QPoint releasePoint = start + QPoint(140, 0);
    QTest::mouseMove(&window, releasePoint, 30);
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("dragTargetIndex").toInt(), 2, 1000);
    // Model the exclusive grab suppressing HoverHandler updates until the
    // release transition. The DragHandler must supply the authoritative point.
    QObject *hoverHandler = panel->findChild<QObject *>(QStringLiteral("dockHoverHandler"));
    QVERIFY(hoverHandler);
    hoverHandler->setProperty("enabled", false);
    QVERIFY(QMetaObject::invokeMethod(panel, "setPointerInside",
                                      Q_ARG(QVariant, QVariant(false))));
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, releasePoint);
    QTRY_VERIFY_WITH_TIMEOUT(!source->property("dragging").toBool(), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("pointerTargetDesktopFileName").toString(),
                              QStringLiteral("three.desktop"), 1000);
    QVERIFY(panel->property("pointerInside").toBool());

    delete panel;
}

void DockHoverQmlTest::reorderLiftsOnlyTheDraggedDelegate()
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
        QCoreApplication::processEvents();

        QVERIFY(QMetaObject::invokeMethod(panel, "beginReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop")))));
        QVERIFY(QMetaObject::invokeMethod(panel, "updateReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop"))),
                                          Q_ARG(QVariant, QVariant(80.0)),
                                          Q_ARG(QVariant, QVariant(0.0)),
                                          Q_ARG(QVariant, QVariant(0.0))));
        QQuickItem *dragged = delegate(panel, QStringLiteral("one.desktop"));
        QVERIFY(dragged);
        QVERIFY(dragged->property("dragging").toBool());
        QTRY_COMPARE_WITH_TIMEOUT(propertyReal(dragged, "visualOffsetY"), -8.0, 1000);
        for (const QString &key : {QStringLiteral("two.desktop"), QStringLiteral("three.desktop")}) {
            QQuickItem *neighbor = delegate(panel, key);
            QVERIFY(neighbor);
            QTRY_COMPARE_WITH_TIMEOUT(propertyReal(neighbor, "visualOffsetY"), 0.0, 1000);
        }
        QVERIFY(QMetaObject::invokeMethod(panel, "cancelReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop")))));
        delete panel;
    }
}

void DockHoverQmlTest::magnifiedInteractionTargetMatchesVisualBounds()
{
    DockController controller;
    DockConfig config = configFor(QStringLiteral("magnification"),
                                  {QStringLiteral("one.desktop")});
    config.iconSize = 64;
    config.magnificationScale = 2.0;
    controller.applyConfig(config);
    FakeContextMenuController contextMenuController;
    DockSurfaceGeometry surfaceGeometry;

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
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
    panel->setProperty("outputWidth", 520);
    panel->setProperty("outputHeight", 220);

    QQuickWindow window;
    window.resize(520, 220);
    panel->setParentItem(window.contentItem());
    panel->setY(20);
    const auto centerPanel = [panel, &window]() {
        panel->setX((window.width() - panel->width()) / 2.0);
    };
    QObject::connect(panel, &QQuickItem::widthChanged, panel, centerPanel);
    centerPanel();
    window.show();
    QTest::qWait(20);

    QQuickItem *delegateItem = delegate(panel, QStringLiteral("one.desktop"));
    QQuickItem *icon = iconItem(delegateItem);
    QQuickItem *interactionTarget = childWithObjectName(panel,
                                                         QStringLiteral("interactionTarget-one.desktop"));
    QVERIFY(delegateItem && icon && interactionTarget);
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(panel->width() / 2.0))));
    QTRY_VERIFY_WITH_TIMEOUT(icon->scale() > 1.99, 1000);

    const QRectF visualRect = icon->mapRectToItem(window.contentItem(),
                                                   QRectF(0, 0, icon->width(), icon->height()));
    const QRectF interactionRect = interactionTarget->mapRectToItem(
        window.contentItem(), QRectF(0, 0, interactionTarget->width(), interactionTarget->height()));
    QVERIFY2(qAbs(visualRect.x() - interactionRect.x()) < 1.0,
             qPrintable(QStringLiteral("visual x=%1 interaction x=%2")
                            .arg(visualRect.x()).arg(interactionRect.x())));
    QVERIFY2(qAbs(visualRect.y() - interactionRect.y()) < 1.0,
             qPrintable(QStringLiteral("visual y=%1 interaction y=%2")
                            .arg(visualRect.y()).arg(interactionRect.y())));
    QVERIFY2(qAbs(visualRect.width() - interactionRect.width()) < 1.0,
             qPrintable(QStringLiteral("visual width=%1 interaction width=%2")
                            .arg(visualRect.width()).arg(interactionRect.width())));
    QVERIFY2(qAbs(visualRect.height() - interactionRect.height()) < 1.0,
             qPrintable(QStringLiteral("visual height=%1 interaction height=%2")
                            .arg(visualRect.height()).arg(interactionRect.height())));

    const QRectF panelRect = panel->mapRectToItem(window.contentItem(),
                                                   QRectF(0, 0, panel->width(), panel->height()));
    QVERIFY(panelRect.top() < visualRect.top());
    const QPoint topPoint(qRound(visualRect.center().x()), qRound(visualRect.top() + 2.0));
    QVERIFY(visualRect.contains(topPoint));
    const QPointF topPanelPoint = panel->mapFromItem(window.contentItem(), topPoint.x(),
                                                      topPoint.y());
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                      Q_ARG(QVariant, QVariant(topPanelPoint.x())),
                                      Q_ARG(QVariant, QVariant(topPanelPoint.y()))));
    QVERIFY(panel->property("pointerInside").toBool());
    QSignalSpy activationSpy(delegateItem, SIGNAL(activated(QString)));
    QVERIFY(activationSpy.isValid());
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, topPoint);
    QTRY_COMPARE_WITH_TIMEOUT(activationSpy.count(), 1, 1000);

    QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier, topPoint);
    QTRY_COMPARE_WITH_TIMEOUT(contextMenuController.presentCount, 1, 1000);

    const QPoint emptyHeadroom(qRound(visualRect.center().x()),
                               qRound(visualRect.top() - 2.0));
    QVERIFY(panelRect.contains(emptyHeadroom));
    QVERIFY(!interactionRect.contains(emptyHeadroom));
    const QPointF emptyPanelPoint = panel->mapFromItem(window.contentItem(),
                                                        emptyHeadroom.x(), emptyHeadroom.y());
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                      Q_ARG(QVariant, QVariant(emptyPanelPoint.x())),
                                      Q_ARG(QVariant, QVariant(emptyPanelPoint.y()))));
    QVERIFY(!panel->property("pointerInside").toBool());
    QCOMPARE(panel->property("pointerTargetDesktopFileName").toString(), QString());
    activationSpy.clear();
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, emptyHeadroom);
    QTest::qWait(50);
    QCOMPARE(activationSpy.count(), 0);

    delete panel;
}

void DockHoverQmlTest::exclusiveDragReleaseInEmptyHeadroomStaysOutsideDock()
{
    DockController controller;
    controller.applyConfig(configFor(QStringLiteral("magnification"),
                                     {QStringLiteral("one.desktop"),
                                      QStringLiteral("two.desktop"),
                                      QStringLiteral("three.desktop")}));

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
    window.resize(600, 220);
    const auto centerPanel = [panel, &window]() {
        panel->setX((window.width() - panel->width()) / 2.0);
    };
    QObject::connect(panel, &QQuickItem::widthChanged, panel, centerPanel);
    centerPanel();
    window.show();
    QTest::qWait(20);

    QQuickItem *source = delegate(panel, QStringLiteral("one.desktop"));
    QQuickItem *icon = iconItem(source);
    QVERIFY(source && icon);
    QSignalSpy activationSpy(source, SIGNAL(activated(QString)));
    QSignalSpy reorderSpy(panel, SIGNAL(reorderRequested(QString,int)));
    QVERIFY(activationSpy.isValid() && reorderSpy.isValid());

    const QPoint start = visualCenter(window, icon).toPoint();
    QTest::mouseMove(&window, start);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&window, start + QPoint(20, 0), 30);
    QTRY_VERIFY_WITH_TIMEOUT(source->property("dragging").toBool(), 1000);

    const QRectF panelRect = panel->mapRectToItem(window.contentItem(),
                                                   QRectF(0, 0, panel->width(), panel->height()));
    const QPoint emptyHeadroom(qRound(panelRect.left() + 1.0),
                               qRound(panelRect.top() + 1.0));
    const QQuickItem *interactionTarget = childWithObjectName(
        panel, QStringLiteral("interactionTarget-one.desktop"));
    QVERIFY(interactionTarget);
    const QRectF interactionRect = interactionTarget->mapRectToItem(
        window.contentItem(), QRectF(0, 0, interactionTarget->width(),
                                     interactionTarget->height()));
    QVERIFY(panelRect.contains(emptyHeadroom));
    QVERIFY(!interactionRect.contains(emptyHeadroom));

    QTest::mouseMove(&window, emptyHeadroom, 30);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, emptyHeadroom);
    QTRY_VERIFY_WITH_TIMEOUT(!source->property("dragging").toBool(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!panel->property("pointerInside").toBool(), 1000);
    QCOMPARE(panel->property("pointerTargetDesktopFileName").toString(), QString());
    QCOMPARE(activationSpy.count(), 0);
    QCOMPARE(reorderSpy.count(), 0);

    delete panel;
}

void DockHoverQmlTest::magnifiedInteractionRegionsResolveByVisualStacking()
{
    DockController controller;
    DockConfig config = configFor(QStringLiteral("magnification"),
                                  {QStringLiteral("one.desktop"),
                                   QStringLiteral("two.desktop"),
                                   QStringLiteral("three.desktop")});
    config.iconSize = 64;
    config.magnificationScale = 2.0;
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
    window.resize(600, 220);
    const auto centerPanel = [panel, &window]() {
        panel->setX((window.width() - panel->width()) / 2.0);
    };
    QObject::connect(panel, &QQuickItem::widthChanged, panel, centerPanel);
    centerPanel();
    window.show();
    QTest::qWait(20);

    QQuickItem *center = delegate(panel, QStringLiteral("two.desktop"));
    QQuickItem *right = delegate(panel, QStringLiteral("three.desktop"));
    QVERIFY(center && right);
    const qreal centerX = center->mapToItem(panel, center->width() / 2.0,
                                            center->height() / 2.0).x();
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(centerX))));
    QTRY_VERIFY_WITH_TIMEOUT(propertyReal(center, "magnificationScale")
                                 > 1.99,
                             1000);
    // The normal spacing-preserving layout avoids accidental overlap. Force a
    // transient visual overlap to exercise the z-order arbitration path.
    right->setProperty("dragging", true);
    right->setProperty("visualOffsetX", -30.0);
    right->setProperty("dragging", false);
    QCoreApplication::processEvents();
    const QRectF centerRect = iconItem(center)->mapRectToItem(
        window.contentItem(), QRectF(0, 0, iconItem(center)->width(), iconItem(center)->height()));
    const QRectF rightRect = iconItem(right)->mapRectToItem(
        window.contentItem(), QRectF(0, 0, iconItem(right)->width(), iconItem(right)->height()));
    QQuickItem *centerTarget = childWithObjectName(panel,
                                                    QStringLiteral("interactionTarget-two.desktop"));
    QQuickItem *rightTarget = childWithObjectName(panel,
                                                   QStringLiteral("interactionTarget-three.desktop"));
    QVERIFY(centerTarget && rightTarget);
    const QRectF overlap = centerRect.intersected(rightRect);
    QVERIFY2(overlap.width() > 1.0 && overlap.height() > 1.0,
             qPrintable(QStringLiteral("center=%1,%2 %3x%4 right=%5,%6 %7x%8")
                            .arg(centerRect.x()).arg(centerRect.y()).arg(centerRect.width())
                            .arg(centerRect.height()).arg(rightRect.x()).arg(rightRect.y())
                            .arg(rightRect.width()).arg(rightRect.height())));

    QSignalSpy centerActivation(center, SIGNAL(activated(QString)));
    QSignalSpy rightActivation(right, SIGNAL(activated(QString)));
    QVERIFY(centerActivation.isValid() && rightActivation.isValid());
    const QPoint intendedPoint(qRound(overlap.left() + 2.0), qRound(overlap.center().y()));
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, intendedPoint);
    QTRY_COMPARE_WITH_TIMEOUT(centerActivation.count(), 1, 1000);
    QCOMPARE(rightActivation.count(), 0);

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
                                          Q_ARG(QVariant, QVariant(140.0)),
                                          Q_ARG(QVariant, QVariant(0.0)),
                                          Q_ARG(QVariant, QVariant(0.0))));
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
