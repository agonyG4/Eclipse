#include "core/ContextMenuController.hpp"
#include "core/ContextMenuModel.hpp"

#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTest>

using Astrea::Shell::ContextMenuController;
using Astrea::Shell::ContextMenuModel;
using Astrea::Shell::ContextMenuTarget;

namespace {

constexpr QLatin1StringView kViewUrl(
    "qrc:/qt/qml/Astrea/Shell/ContextMenu/qml/ContextMenuView.qml");
constexpr QLatin1StringView kOverlayUrl(
    "qrc:/qt/qml/Astrea/Shell/ContextMenu/qml/ContextMenuOverlaySurface.qml");

ContextMenuTarget desktopTarget()
{
    return {ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
            QStringLiteral("test-output")};
}

QQuickItem *createView(QQmlEngine &engine, QQuickWindow &window,
                       ContextMenuController &controller)
{
    QQmlComponent component(&engine, QUrl(kViewUrl));
    if (component.status() != QQmlComponent::Ready)
        return nullptr;
    auto *view = qobject_cast<QQuickItem *>(component.createWithInitialProperties({
        {QStringLiteral("menuModel"), QVariant::fromValue(controller.model())},
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("presentationGeneration"), controller.presentationGeneration()},
        {QStringLiteral("outputWidth"), 280},
        {QStringLiteral("outputHeight"), 180},
    }));
    if (!view)
        return nullptr;
    view->setParentItem(window.contentItem());
    view->setWidth(280);
    window.setWidth(280);
    window.setHeight(180);
    view->forceActiveFocus();
    window.show();
    QCoreApplication::processEvents();
    return view;
}

QQuickItem *loadedChild(QQuickItem *view)
{
    for (QObject *object : view->findChildren<QObject *>()) {
        const QVariant itemValue = object->property("item");
        if (!itemValue.canConvert<QObject *>())
            continue;
        if (auto *item = qobject_cast<QQuickItem *>(itemValue.value<QObject *>()))
            return item;
    }
    return nullptr;
}

} // namespace

class ContextMenuQmlInteractionTest final : public QObject {
    Q_OBJECT

private slots:
    void keyboardNavigationSkipsDisabledAndSeparators();
    void keyboardActivationDispatchesLeafAction();
    void leftRestoresParentFocusAndSelection();
    void scrolledSubmenuUsesVisibleRowGeometry();
    void outsidePressClosesButInsideDisabledRowDoesNotHitShield();
};

void ContextMenuQmlInteractionTest::keyboardNavigationSkipsDisabledAndSeparators()
{
    ContextMenuController controller;
    QVERIFY(controller.present(desktopTarget(), {
        {.token = QStringLiteral("first"), .label = QStringLiteral("First")},
        {.kind = ContextMenuModel::NodeKind::Separator},
        {.token = QStringLiteral("disabled"), .label = QStringLiteral("Disabled"),
         .enabled = false},
        {.token = QStringLiteral("second"), .label = QStringLiteral("Second")},
        {.kind = ContextMenuModel::NodeKind::Submenu, .label = QStringLiteral("More"),
         .children = {{.token = QStringLiteral("child"), .label = QStringLiteral("Child")}}},
    }, [](const QString &) { return true; }));

    QQmlEngine engine;
    QQuickWindow window;
    auto *view = createView(engine, window, controller);
    QVERIFY(view);
    QTRY_COMPARE(view->property("activeIndex").toInt(), 0);

    QTest::keyClick(&window, Qt::Key_Down);
    QCOMPARE(view->property("activeIndex").toInt(), 3);
    QTest::keyClick(&window, Qt::Key_Home);
    QCOMPARE(view->property("activeIndex").toInt(), 0);
    QTest::keyClick(&window, Qt::Key_End);
    QCOMPARE(view->property("activeIndex").toInt(), 4);
    delete view;
}

void ContextMenuQmlInteractionTest::keyboardActivationDispatchesLeafAction()
{
    ContextMenuController controller;
    int activations = 0;
    QVERIFY(controller.present(desktopTarget(), {
        {.token = QStringLiteral("action"), .label = QStringLiteral("Action")},
    }, [&](const QString &token) {
        ++activations;
        return token == QStringLiteral("action");
    }));

    QQmlEngine engine;
    QQuickWindow window;
    auto *view = createView(engine, window, controller);
    QVERIFY(view);
    QTest::keyClick(&window, Qt::Key_Space);
    QCOMPARE(activations, 1);
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closing);
    delete view;
}

void ContextMenuQmlInteractionTest::leftRestoresParentFocusAndSelection()
{
    ContextMenuController controller;
    QVERIFY(controller.present(desktopTarget(), {
        {.token = QStringLiteral("first"), .label = QStringLiteral("First")},
        {.kind = ContextMenuModel::NodeKind::Submenu, .label = QStringLiteral("More"),
         .children = {{.token = QStringLiteral("child"), .label = QStringLiteral("Child")}}},
        {.token = QStringLiteral("last"), .label = QStringLiteral("Last")},
    }, [](const QString &) { return true; }));

    QQmlEngine engine;
    QQuickWindow window;
    auto *view = createView(engine, window, controller);
    QVERIFY(view);
    QTest::keyClick(&window, Qt::Key_Down);
    QCOMPARE(view->property("activeIndex").toInt(), 1);
    QTest::keyClick(&window, Qt::Key_Right);
    QQuickItem *child = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((child = loadedChild(view)) != nullptr, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(window.activeFocusItem() == child, 1000);

    QTest::keyClick(&window, Qt::Key_Left);
    QTRY_VERIFY_WITH_TIMEOUT(window.activeFocusItem() == view, 1000);
    QCOMPARE(view->property("activeIndex").toInt(), 1);
    QTest::keyClick(&window, Qt::Key_Down);
    QCOMPARE(view->property("activeIndex").toInt(), 2);
    delete view;
}

void ContextMenuQmlInteractionTest::scrolledSubmenuUsesVisibleRowGeometry()
{
    ContextMenuController controller;
    QVector<ContextMenuModel::NodeSpec> nodes;
    for (int i = 0; i < 20; ++i) {
        nodes.append({.token = QStringLiteral("action-%1").arg(i),
                      .label = QStringLiteral("Action %1").arg(i)});
    }
    nodes.append({.kind = ContextMenuModel::NodeKind::Submenu, .label = QStringLiteral("More"),
                  .children = {{.token = QStringLiteral("child"), .label = QStringLiteral("Child")}}});
    QVERIFY(controller.present(desktopTarget(), nodes, [](const QString &) { return true; }));

    QQmlEngine engine;
    QQuickWindow window;
    auto *view = createView(engine, window, controller);
    QVERIFY(view);
    QTest::keyClick(&window, Qt::Key_End);
    QCOMPARE(view->property("activeIndex").toInt(), 20);
    const QRectF row = view->property("activeRowRectangle").toRectF();
    QVERIFY(row.height() > 0);
    QVERIFY(row.y() >= 0);
    QVERIFY(row.bottom() <= window.height());
    QTest::keyClick(&window, Qt::Key_Right);
    QTRY_VERIFY_WITH_TIMEOUT(loadedChild(view) != nullptr, 1000);
    delete view;
}

void ContextMenuQmlInteractionTest::outsidePressClosesButInsideDisabledRowDoesNotHitShield()
{
    ContextMenuController controller;
    QVERIFY(controller.present(desktopTarget(), {
        {.token = QStringLiteral("disabled"), .label = QStringLiteral("Disabled"),
         .enabled = false},
    }, [](const QString &) { return true; }));

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(kOverlayUrl));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties({
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("outputKey"), QStringLiteral("test-output")},
        {QStringLiteral("outputWidth"), 280},
        {QStringLiteral("outputHeight"), 180},
    }));
    QVERIFY(window);
    window->setWidth(280);
    window->setHeight(180);
    window->show();
    QCoreApplication::processEvents();
    const auto menuView = window->findChild<QObject *>(QStringLiteral("contextMenuView"));
    QVERIFY(menuView);
    const int insideX = menuView->property("x").toInt() + 20;
    const int insideY = menuView->property("y").toInt() + 20;
    QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(insideX, insideY));
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Open);
    QTest::mouseClick(window, Qt::RightButton, {}, QPoint(window->width() - 1,
                                                          window->height() - 1));
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closing);
    controller.completeClose();
    delete window;
}

QTEST_MAIN(ContextMenuQmlInteractionTest)
#include "ContextMenuQmlInteractionTest.moc"
