#include "core/ContextMenuController.hpp"
#include "core/ContextMenuModel.hpp"
#include "statusnotifier/DBusMenuModel.hpp"

#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTest>

using Astrea::Shell::ContextMenuController;
using Astrea::Shell::ContextMenuModel;
using Astrea::Shell::ContextMenuTarget;

class FakeTrayMenuService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(quint64 presentationRevision READ presentationRevision
                   NOTIFY presentationRevisionChanged)

public:
    FakeTrayMenuService()
        : m_model(this)
    {
    }

    Astrea::StatusNotifier::DBusMenuModel *model() { return &m_model; }
    quint64 presentationRevision() const { return m_revision; }
    int prepareCount() const { return m_prepareCount; }

    Q_INVOKABLE bool hasMenuForItem(const QString &key) const
    {
        return key == QStringLiteral("tray-test");
    }

    Q_INVOKABLE bool hasUsableMenuForItem(const QString &key) const
    {
        return hasMenuForItem(key);
    }

    Q_INVOKABLE QObject *menuModelForItem(const QString &key) const
    {
        return hasMenuForItem(key) ? const_cast<Astrea::StatusNotifier::DBusMenuModel *>(&m_model)
                                   : nullptr;
    }

    Q_INVOKABLE int menuStateForItem(const QString &key) const
    {
        return hasMenuForItem(key) ? 3 : 0;
    }

    Q_INVOKABLE QString displayTitleForItem(const QString &key) const
    {
        return hasMenuForItem(key) ? QStringLiteral("Test tray") : QString();
    }

    Q_INVOKABLE QString iconSourceForItem(const QString &) const { return {}; }

    Q_INVOKABLE void prepareMenuForPresentation(const QString &key, int = 0)
    {
        if (!hasMenuForItem(key))
            return;
        ++m_prepareCount;
        ++m_revision;
        emit presentationRevisionChanged();
        emit menuContentChanged(key);
    }

    Q_INVOKABLE void aboutToShowMenu(const QString &key, int nodeId)
    {
        prepareMenuForPresentation(key, nodeId);
        emit aboutToShowRequested(nodeId);
    }

signals:
    void presentationRevisionChanged();
    void menuClientChanged(const QString &key);
    void menuContentChanged(const QString &key);
    void aboutToShowRequested(int nodeId);

private:
    Astrea::StatusNotifier::DBusMenuModel m_model;
    quint64 m_revision = 0;
    int m_prepareCount = 0;
};

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
    void closingDisablesInputAndCompletesAfterAnimation();
    void globalOverlayKeepsTrayMenuLiveAndDispatchable();
    void trayPlacementUsesResolvedRenderedWidth();
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

void ContextMenuQmlInteractionTest::closingDisablesInputAndCompletesAfterAnimation()
{
    ContextMenuController controller;
    QVERIFY(controller.present(desktopTarget(), {
        {.token = QStringLiteral("settings"), .label = QStringLiteral("Settings")},
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
    QObject *menu = window->findChild<QObject *>(QStringLiteral("contextMenuView"));
    QVERIFY(menu);
    QTRY_VERIFY_WITH_TIMEOUT(menu->property("opacity").toReal() > 0.99, 1000);

    controller.close();
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closing);
    QVERIFY(!menu->property("enabled").toBool());
    QTRY_COMPARE_WITH_TIMEOUT(controller.lifecycle(), ContextMenuController::Lifecycle::Closed, 1000);
    delete window;
}

void ContextMenuQmlInteractionTest::globalOverlayKeepsTrayMenuLiveAndDispatchable()
{
    FakeTrayMenuService service;
    Astrea::StatusNotifier::DBusMenuNode root;
    root.children = {
        {.id = 7, .label = QStringLiteral("Remote action"), .enabled = true, .visible = true},
        {.id = 8, .label = QStringLiteral("Remote submenu"), .childrenDisplay = QStringLiteral("submenu"),
         .enabled = true, .visible = true,
         .children = {{.id = 9, .label = QStringLiteral("Nested submenu"),
                       .children = {{.id = 10, .label = QStringLiteral("Deep action")}}}}},
    };
    service.model()->setRoot(root);

    ContextMenuController controller;
    controller.setTrayService(&service);
    QSignalSpy activationSpy(service.model(),
                             &Astrea::StatusNotifier::DBusMenuModel::activateRequested);
    const ContextMenuTarget target{ContextMenuTarget::Kind::TrayItem,
                                   QStringLiteral("tray-test"), QStringLiteral("test-output")};
    QVERIFY(controller.present(target, {}, [&](const QString &token) {
        bool ok = false;
        const int nodeId = token.mid(QStringLiteral("tray.node.").size()).toInt(&ok);
        if (!ok)
            return false;
        service.model()->activate(nodeId);
        return true;
    }, [] { return true; }, [&](const QString &token) {
        bool ok = false;
        const int nodeId = token.mid(QStringLiteral("tray.node.").size()).toInt(&ok);
        const auto node = service.model()->nodeById(nodeId);
        return token.startsWith(QStringLiteral("tray.node.")) && ok && node.id == nodeId
            && node.visible && node.enabled && !node.separator && node.children.isEmpty();
    }));

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(kOverlayUrl));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties({
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("trayService"), QVariant::fromValue(&service)},
        {QStringLiteral("outputKey"), QStringLiteral("test-output")},
        {QStringLiteral("outputWidth"), 280},
        {QStringLiteral("outputHeight"), 180},
    }));
    QVERIFY(window);
    window->setWidth(280);
    window->setHeight(180);
    window->show();
    QObject *menu = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((menu = window->findChild<QObject *>(
                                  QStringLiteral("trayContextMenu"))) != nullptr, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(menu->property("menuModel").value<QObject *>(),
                              static_cast<QObject *>(service.model()), 1000);
    QVERIFY(service.prepareCount() > 0);
    QTRY_VERIFY_WITH_TIMEOUT(window->activeFocusItem() != nullptr, 1000);
    QCOMPARE(window->activeFocusItem(), qobject_cast<QQuickItem *>(menu));

    QTest::keyClick(window, Qt::Key_Down);
    QTest::keyClick(window, Qt::Key_Right);
    QObject *childMenu = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((childMenu = window->findChild<QObject *>(
                                  QStringLiteral("trayCascadeMenu"))) != nullptr, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->activeFocusItem(),
                              qobject_cast<QQuickItem *>(childMenu), 1000);
    QTest::keyClick(window, Qt::Key_Right);
    QTRY_VERIFY_WITH_TIMEOUT(window->findChildren<QObject *>(
                                 QStringLiteral("trayCascadeMenu")).size() == 2, 1000);
    const auto cascades = window->findChildren<QObject *>(QStringLiteral("trayCascadeMenu"));
    QObject *deepMenu = cascades.constLast();
    QTRY_COMPARE_WITH_TIMEOUT(window->activeFocusItem(),
                              qobject_cast<QQuickItem *>(deepMenu), 1000);
    QTest::keyClick(window, Qt::Key_Left);
    QTRY_COMPARE_WITH_TIMEOUT(window->activeFocusItem(),
                              qobject_cast<QQuickItem *>(childMenu), 1000);
    QTest::keyClick(window, Qt::Key_Left);
    QTRY_COMPARE_WITH_TIMEOUT(window->activeFocusItem(),
                              qobject_cast<QQuickItem *>(menu), 1000);
    QCOMPARE(menu->property("activeIndex").toInt(), 1);

    QTest::keyClick(window, Qt::Key_Down);
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_COMPARE_WITH_TIMEOUT(activationSpy.count(), 1, 1000);
    QCOMPARE(activationSpy.at(0).at(0).toInt(), 7);
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closing);
    controller.completeClose();
    delete window;
}

void ContextMenuQmlInteractionTest::trayPlacementUsesResolvedRenderedWidth()
{
    FakeTrayMenuService service;
    Astrea::StatusNotifier::DBusMenuNode action;
    action.id = 7;
    action.label = QStringLiteral("Remote action");
    service.model()->setNodes({action});

    ContextMenuController controller;
    const ContextMenuTarget target{ContextMenuTarget::Kind::TrayItem,
                                   QStringLiteral("tray-test"), QStringLiteral("test-output")};
    QVERIFY(controller.present(target, {}, [](const QString &) { return true; }));

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(kOverlayUrl));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties({
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("trayService"), QVariant::fromValue(&service)},
        {QStringLiteral("outputKey"), QStringLiteral("test-output")},
        {QStringLiteral("outputWidth"), 280},
        {QStringLiteral("outputHeight"), 180},
    }));
    QVERIFY(window);
    window->setWidth(280);
    window->setHeight(180);
    window->show();

    QObject *menu = nullptr;
    QObject *loader = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((menu = window->findChild<QObject *>(
                                  QStringLiteral("trayContextMenu"))) != nullptr, 1000);
    QTRY_VERIFY_WITH_TIMEOUT((loader = window->findChild<QObject *>(
                                  QStringLiteral("trayMenuLoader"))) != nullptr, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(menu->property("width").toReal()
                                      - loader->property("width").toReal()) < 0.01,
                             1000);
    QCOMPARE(qRound(menu->property("width").toReal()),
             qRound(menu->property("implicitWidth").toReal()));
    QCOMPARE(qRound(menu->property("width").toReal()),
             qRound(loader->property("width").toReal()));
    const QPoint expected = controller.menuPosition(
        280, 180, qRound(loader->property("width").toReal()),
        qRound(loader->property("height").toReal()));
    QCOMPARE(QPoint(loader->property("x").toInt(), loader->property("y").toInt()), expected);

    controller.completeClose();
    delete window;
}

QTEST_MAIN(ContextMenuQmlInteractionTest)
#include "ContextMenuQmlInteractionTest.moc"
