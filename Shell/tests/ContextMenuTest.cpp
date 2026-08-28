#include "core/ContextMenuController.hpp"
#include "core/ContextMenuModel.hpp"
#include "core/ContextMenuPlacement.hpp"
#include "core/ContextMenuProviders.hpp"
#include "core/ContextMenuSurfacePolicy.hpp"
#include "core/ContextMenuSurfaceMapping.hpp"
#include "statusnotifier/DBusMenuModel.hpp"
#include "statusnotifier/StatusNotifierService.hpp"

#include <QSignalSpy>
#include <QtTest/QtTest>

using Astrea::Shell::ContextMenuController;
using Astrea::Shell::ContextMenuModel;
using Astrea::Shell::ContextMenuPlacement;
using Astrea::Shell::ContextMenuTarget;

class ContextMenuTest final : public QObject {
    Q_OBJECT

private slots:
    void controllerStartsClosed();
    void controllerOwnsGenerationAndLifecycle();
    void controllerRejectsStaleAndInvalidActions();
    void controllerDispatchesDynamicTrayAction();
    void trayAdapterRejectsLazySubmenuActions();
    void controllerClosesWhenTargetValidatorRejects();
    void controllerSettlesWhenOutputIsRemoved();
    void overlayMappingIsOutputScoped();
    void controllerShutdownCleansUpActivePresentation();
    void modelNormalizesSeparatorsAndExposesRoles();
    void modelPresentationMetricsAreExactAndContentAware();
    void modelRejectsDepthAndNodeBounds();
    void placementFlipsAndClampsInOutputLocalCoordinates();
    void surfacePoliciesKeepInputAndLayerContracts();
};

void ContextMenuTest::controllerStartsClosed()
{
    ContextMenuController controller;

    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closed);
    QCOMPARE(controller.presentationGeneration(), quint64(0));
    QVERIFY(!controller.hasActivePresentation());
}

void ContextMenuTest::controllerOwnsGenerationAndLifecycle()
{
    ContextMenuController controller;
    ContextMenuTarget target{ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
                             QStringLiteral("output-1")};
    QVector<ContextMenuModel::NodeSpec> nodes{{.token = QStringLiteral("settings"),
                                               .label = QStringLiteral("Settings")}};

    QVERIFY(controller.present(target, nodes, [](const QString &) { return true; }));
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Open);
    QCOMPARE(controller.presentationGeneration(), quint64(1));
    QVERIFY(controller.hasActivePresentation());

    controller.close();
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closing);
    controller.close();
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closing);

    controller.completeClose();
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closed);
    QVERIFY(!controller.hasActivePresentation());
}

void ContextMenuTest::controllerRejectsStaleAndInvalidActions()
{
    ContextMenuController controller;
    int activations = 0;
    ContextMenuTarget target{ContextMenuTarget::Kind::DockApplication,
                             QStringLiteral("org.example.App"), QStringLiteral("output-1")};
    QVector<ContextMenuModel::NodeSpec> nodes{
        {.token = QStringLiteral("enabled"), .label = QStringLiteral("Enabled")},
        {.token = QStringLiteral("disabled"), .label = QStringLiteral("Disabled"), .enabled = false},
        {.token = QStringLiteral("hidden"), .label = QStringLiteral("Hidden"), .visible = false},
    };

    QVERIFY(controller.present(target, nodes, [&](const QString &) {
        ++activations;
        return true;
    }));
    const quint64 firstGeneration = controller.presentationGeneration();
    QVERIFY(controller.activate(firstGeneration, QStringLiteral("enabled")));
    QVERIFY(!controller.activate(firstGeneration, QStringLiteral("disabled")));
    QVERIFY(!controller.activate(firstGeneration, QStringLiteral("hidden")));
    QVERIFY(!controller.activate(firstGeneration, QStringLiteral("missing")));
    QCOMPARE(activations, 1);

    QVERIFY(controller.present(target, nodes, [&](const QString &) {
        ++activations;
        return true;
    }));
    QVERIFY(controller.presentationGeneration() > firstGeneration);
    QVERIFY(!controller.activate(firstGeneration, QStringLiteral("enabled")));

    controller.invalidateTarget();
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closing);
    QVERIFY(!controller.activate(controller.presentationGeneration(), QStringLiteral("enabled")));
}

void ContextMenuTest::controllerDispatchesDynamicTrayAction()
{
    ContextMenuController controller;
    bool dispatched = false;
    Astrea::StatusNotifier::DBusMenuModel menu;
    Astrea::StatusNotifier::DBusMenuNode node;
    node.id = 7;
    node.label = QStringLiteral("Open");
    menu.setNodes({node});
    QSignalSpy menuActivations(&menu, &Astrea::StatusNotifier::DBusMenuModel::activateRequested);
    const ContextMenuTarget target{ContextMenuTarget::Kind::TrayItem,
                                   QStringLiteral("tray-item"), QStringLiteral("output-1")};

    QVERIFY(controller.present(target, {}, [&](const QString &token) {
        if (token != QStringLiteral("tray.node.7"))
            return false;
        menu.activate(7);
        dispatched = true;
        return true;
    }, [] { return true; }, [&](const QString &token) {
        if (token != QStringLiteral("tray.node.7"))
            return false;
        const auto current = menu.nodeById(7);
        return current.id == 7 && current.visible && current.enabled
            && !current.separator && current.children.isEmpty();
    }));

    QVERIFY(controller.activate(controller.presentationGeneration(),
                                QStringLiteral("tray.node.7")));
    QVERIFY(dispatched);
    QCOMPARE(menuActivations.size(), 1);
}

void ContextMenuTest::trayAdapterRejectsLazySubmenuActions()
{
    Astrea::StatusNotifier::StatusNotifierService service;
    Astrea::StatusNotifier::ItemSnapshot snapshot;
    snapshot.address = {QStringLiteral("org.example.Tray"),
                        QStringLiteral("/StatusNotifierItem"), QStringLiteral(":1.42")};
    snapshot.id = QStringLiteral("tray");
    snapshot.menuPath = QStringLiteral("/Menu");
    snapshot.generation = 1;
    service.upsertTestItem(snapshot);

    const QString key = snapshot.address.key();
    auto *menu = qobject_cast<Astrea::StatusNotifier::DBusMenuModel *>(
        service.menuModelForItem(key));
    QVERIFY(menu);
    Astrea::StatusNotifier::DBusMenuNode lazySubmenu;
    lazySubmenu.id = 7;
    lazySubmenu.label = QStringLiteral("Lazy submenu");
    lazySubmenu.childrenDisplay = QStringLiteral("submenu");
    menu->setNodes({lazySubmenu});

    Astrea::Shell::ContextMenuController controller;
    Astrea::Shell::TrayContextMenuAdapter adapter(&service);
    Astrea::Shell::ContextMenuAnchor anchor;
    anchor.kind = Astrea::Shell::ContextMenuAnchor::Kind::Rectangle;
    anchor.rectangle = QRect(100, 20, 28, 28);
    anchor.preferredTop = 54;
    QVERIFY(adapter.present(&controller, key, anchor, QStringLiteral("output-1")));
    const quint64 generation = controller.presentationGeneration();
    QCOMPARE(controller.menuPosition(400, 300, 120, 80), QPoint(54, 54));
    QSignalSpy activationSpy(menu, &Astrea::StatusNotifier::DBusMenuModel::activateRequested);

    QVERIFY(!controller.activate(generation, QStringLiteral("tray.node.7")));
    QCOMPARE(activationSpy.count(), 0);
    QCOMPARE(controller.lifecycle(), Astrea::Shell::ContextMenuController::Lifecycle::Open);

    Astrea::StatusNotifier::DBusMenuNode child;
    child.id = 8;
    child.label = QStringLiteral("Child action");
    lazySubmenu.children = {child};
    menu->setNodes({lazySubmenu});
    QVERIFY(!controller.activate(generation, QStringLiteral("tray.node.7")));
    QCOMPARE(activationSpy.count(), 0);

    Astrea::StatusNotifier::DBusMenuNode disabledLeaf;
    disabledLeaf.id = 9;
    disabledLeaf.label = QStringLiteral("Disabled action");
    disabledLeaf.enabled = false;
    menu->setNodes({disabledLeaf});
    QVERIFY(!controller.activate(generation, QStringLiteral("tray.node.9")));
    QCOMPARE(activationSpy.count(), 0);

    Astrea::StatusNotifier::DBusMenuNode leaf;
    leaf.id = 10;
    leaf.label = QStringLiteral("Leaf action");
    menu->setNodes({leaf});
    QVERIFY(!controller.activate(generation, QStringLiteral("tray.node.7")));
    QCOMPARE(activationSpy.count(), 0);

    QVERIFY(adapter.present(&controller, key, anchor, QStringLiteral("output-1")));
    const quint64 currentGeneration = controller.presentationGeneration();
    QVERIFY(currentGeneration > generation);
    QVERIFY(!controller.activate(generation, QStringLiteral("tray.node.10")));
    QVERIFY(controller.activate(currentGeneration, QStringLiteral("tray.node.10")));
    QCOMPARE(activationSpy.count(), 1);
    QCOMPARE(activationSpy.at(0).at(0).toInt(), 10);
}

void ContextMenuTest::controllerClosesWhenTargetValidatorRejects()
{
    ContextMenuController controller;
    bool targetLive = true;
    const ContextMenuTarget target{ContextMenuTarget::Kind::DockApplication,
                                   QStringLiteral("app.desktop"), QStringLiteral("output-1")};
    const QVector<ContextMenuModel::NodeSpec> nodes{
        {.token = QStringLiteral("action"), .label = QStringLiteral("Action")}};

    QVERIFY(controller.present(target, nodes, [](const QString &) { return true; },
                               [&targetLive] { return targetLive; }));
    targetLive = false;
    QVERIFY(!controller.activate(controller.presentationGeneration(), QStringLiteral("action")));
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closing);
}

void ContextMenuTest::controllerSettlesWhenOutputIsRemoved()
{
    ContextMenuController controller;
    QVERIFY(controller.present(
        ContextMenuTarget{ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
                          QStringLiteral("output-A")},
        {ContextMenuModel::NodeSpec{.token = QStringLiteral("settings"),
                                    .label = QStringLiteral("Settings")}},
        [](const QString &) { return true; }));

    controller.invalidateOutput(QStringLiteral("output-A"));
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closed);
    QVERIFY(controller.targetIdentity().isEmpty());
    QVERIFY(controller.model()->rowCount() == 0);
}

void ContextMenuTest::overlayMappingIsOutputScoped()
{
    struct FakeBundle {
        QString outputKey;
        bool mapped = true;
        bool overlayMapped = false;

        void sync(const ContextMenuController &controller)
        {
            overlayMapped = Astrea::Shell::ContextMenuSurfaceMapping::overlayShouldMap(
                outputKey, mapped, controller.hasActivePresentation(), controller.outputKey());
        }
    } outputA{QStringLiteral("output-A")}, outputB{QStringLiteral("output-B")};

    ContextMenuController controller;
    const auto nodes = QVector<ContextMenuModel::NodeSpec>{
        {.token = QStringLiteral("action"), .label = QStringLiteral("Action")}};
    QVERIFY(controller.present(
        ContextMenuTarget{ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
                          QStringLiteral("output-A")}, nodes,
        [](const QString &) { return true; }));
    outputA.sync(controller);
    outputB.sync(controller);
    QVERIFY(outputA.overlayMapped);
    QVERIFY(!outputB.overlayMapped);

    QVERIFY(controller.present(
        ContextMenuTarget{ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
                          QStringLiteral("output-B")}, nodes,
        [](const QString &) { return true; }));
    outputA.sync(controller);
    outputB.sync(controller);
    QVERIFY(!outputA.overlayMapped);
    QVERIFY(outputB.overlayMapped);

    controller.close();
    controller.completeClose();
    outputA.sync(controller);
    outputB.sync(controller);
    QVERIFY(!outputA.overlayMapped);
    QVERIFY(!outputB.overlayMapped);

    QVERIFY(controller.present(
        ContextMenuTarget{ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
                          QStringLiteral("output-A")}, nodes,
        [](const QString &) { return true; }));
    controller.invalidateOutput(QStringLiteral("output-B"));
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Open);
    controller.invalidateOutput(QStringLiteral("output-A"));
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closed);
}

void ContextMenuTest::controllerShutdownCleansUpActivePresentation()
{
    ContextMenuController controller;
    QVERIFY(controller.present(
        ContextMenuTarget{ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
                          QStringLiteral("output-1")},
        {ContextMenuModel::NodeSpec{.token = QStringLiteral("settings"),
                                    .label = QStringLiteral("Settings")}},
        [](const QString &) { return true; }));
    controller.shutdown();
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closed);
    QVERIFY(!controller.hasActivePresentation());
    QVERIFY(controller.targetIdentity().isEmpty());
    controller.close();
    controller.completeClose();
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closed);
}

void ContextMenuTest::modelNormalizesSeparatorsAndExposesRoles()
{
    ContextMenuModel model;
    QVector<ContextMenuModel::NodeSpec> nodes{
        {.kind = ContextMenuModel::NodeKind::Separator},
        {.token = QStringLiteral("one"), .label = QStringLiteral("One"),
         .icon = QStringLiteral("one-symbolic"), .shortcut = QStringLiteral("Ctrl+O"),
         .destructive = true},
        {.kind = ContextMenuModel::NodeKind::Separator},
        {.kind = ContextMenuModel::NodeKind::Separator},
        {.kind = ContextMenuModel::NodeKind::Submenu, .label = QStringLiteral("More"),
         .children = {{.token = QStringLiteral("two"), .label = QStringLiteral("Two"),
                       .checkState = Qt::Checked,
                       .checkType = ContextMenuModel::CheckType::Radio}}},
        {.kind = ContextMenuModel::NodeKind::Separator},
    };

    QVERIFY(model.setRootNodes(nodes));
    QCOMPARE(model.rowCount(), 3);
    const QModelIndex action = model.index(0, 0);
    QCOMPARE(action.data(ContextMenuModel::TokenRole).toString(), QStringLiteral("one"));
    QCOMPARE(action.data(ContextMenuModel::LabelRole).toString(), QStringLiteral("One"));
    QCOMPARE(action.data(ContextMenuModel::IconRole).toString(), QStringLiteral("one-symbolic"));
    QCOMPARE(action.data(ContextMenuModel::ShortcutRole).toString(), QStringLiteral("Ctrl+O"));
    QVERIFY(action.data(ContextMenuModel::DestructiveRole).toBool());

    const QModelIndex submenu = model.index(2, 0);
    QVERIFY(submenu.data(ContextMenuModel::HasChildrenRole).toBool());
    QCOMPARE(model.rowCount(submenu), 1);
    const QModelIndex child = model.index(0, 0, submenu);
    QCOMPARE(child.data(ContextMenuModel::CheckTypeRole).toInt(),
             int(ContextMenuModel::CheckType::Radio));
    QCOMPARE(child.data(ContextMenuModel::CheckStateRole).toInt(), int(Qt::Checked));
    QCOMPARE(model.firstNavigable(), 0);
    QCOMPARE(model.nextNavigable(0, 1), 2);
    QCOMPARE(model.nextNavigable(-1, -1), 2);

    ContextMenuModel hiddenSeparatorModel;
    QVERIFY(hiddenSeparatorModel.setRootNodes({
        {.kind = ContextMenuModel::NodeKind::Separator},
        {.token = QStringLiteral("hidden"), .label = QStringLiteral("Hidden"),
         .visible = false},
        {.kind = ContextMenuModel::NodeKind::Separator},
        {.token = QStringLiteral("visible"), .label = QStringLiteral("Visible")},
        {.kind = ContextMenuModel::NodeKind::Separator},
    }));
    QCOMPARE(hiddenSeparatorModel.rowCount(), 2);
    QCOMPARE(hiddenSeparatorModel.index(0, 0).data(ContextMenuModel::KindRole).toInt(),
             int(ContextMenuModel::NodeKind::Action));
    QCOMPARE(hiddenSeparatorModel.index(1, 0).data(ContextMenuModel::KindRole).toInt(),
             int(ContextMenuModel::NodeKind::Action));
}

void ContextMenuTest::modelPresentationMetricsAreExactAndContentAware()
{
    const auto action = [](const QString &token, const QString &label) {
        return ContextMenuModel::NodeSpec{.token = token, .label = label};
    };

    ContextMenuModel one;
    QVERIFY(one.setRootNodes({action(QStringLiteral("one"), QStringLiteral("One"))}));
    QCOMPARE(one.presentationContentHeight(36, 10), 36);

    ContextMenuModel three;
    QVERIFY(three.setRootNodes({
        action(QStringLiteral("one"), QStringLiteral("One")),
        action(QStringLiteral("two"), QStringLiteral("Two")),
        action(QStringLiteral("three"), QStringLiteral("Three")),
    }));
    QCOMPARE(three.presentationContentHeight(36, 10), 108);

    ContextMenuModel mixed;
    QVERIFY(mixed.setRootNodes({
        action(QStringLiteral("one"), QStringLiteral("One")),
        action(QStringLiteral("two"), QStringLiteral("Two")),
        ContextMenuModel::NodeSpec{.kind = ContextMenuModel::NodeKind::Separator},
        action(QStringLiteral("three"), QStringLiteral("Three")),
    }));
    QCOMPARE(mixed.presentationContentHeight(36, 10), 118);

    ContextMenuModel hidden;
    QVERIFY(hidden.setRootNodes({
        action(QStringLiteral("visible"), QStringLiteral("Visible")),
        {.token = QStringLiteral("hidden"), .label = QStringLiteral("Hidden"), .visible = false},
    }));
    QCOMPARE(hidden.presentationContentHeight(36, 10), 36);

    ContextMenuModel submenu;
    QVERIFY(submenu.setRootNodes({ContextMenuModel::NodeSpec{
        .kind = ContextMenuModel::NodeKind::Submenu,
        .label = QStringLiteral("More"),
        .children = {action(QStringLiteral("child"), QStringLiteral("Child"))},
    }}));
    QCOMPARE(submenu.presentationContentHeight(36, 10), 36);

    const auto naturalWidth = [](ContextMenuModel &model) {
        return model.presentationNaturalWidth(QStringLiteral("Inter Variable"), 12, 12,
                                              10, 20, 12, 10, 1);
    };
    ContextMenuModel shortLabel;
    QVERIFY(shortLabel.setRootNodes({action(QStringLiteral("short"), QStringLiteral("Short"))}));
    const int shortWidth = naturalWidth(shortLabel);

    ContextMenuModel shortcut;
    auto shortcutNode = action(QStringLiteral("shortcut"), QStringLiteral("Short"));
    shortcutNode.shortcut = QStringLiteral("Ctrl+Shift+P");
    QVERIFY(shortcut.setRootNodes({shortcutNode}));
    QVERIFY(naturalWidth(shortcut) > shortWidth);

    ContextMenuModel submenuWidth;
    QVERIFY(submenuWidth.setRootNodes({ContextMenuModel::NodeSpec{
        .kind = ContextMenuModel::NodeKind::Submenu,
        .label = QStringLiteral("Short"),
        .children = {action(QStringLiteral("child"), QStringLiteral("Child"))},
    }}));
    QVERIFY(naturalWidth(submenuWidth) > shortWidth);

    ContextMenuModel longLabel;
    QVERIFY(longLabel.setRootNodes({action(
        QStringLiteral("long"),
        QStringLiteral("A deliberately long context menu label that needs to be bounded"))}));
    QVERIFY(naturalWidth(longLabel) > naturalWidth(shortLabel));

    ContextMenuModel hiddenWidth;
    QVERIFY(hiddenWidth.setRootNodes({
        action(QStringLiteral("short"), QStringLiteral("Short")),
        {.token = QStringLiteral("hidden"),
         .label = QStringLiteral("A hidden label that must not affect width"),
         .visible = false},
        {.kind = ContextMenuModel::NodeKind::Separator},
    }));
    QCOMPARE(naturalWidth(hiddenWidth), shortWidth);
}

void ContextMenuTest::modelRejectsDepthAndNodeBounds()
{
    ContextMenuModel model;
    QVector<ContextMenuModel::NodeSpec> tooMany;
    for (int i = 0; i < 257; ++i) {
        tooMany.push_back({.token = QStringLiteral("action-%1").arg(i),
                           .label = QStringLiteral("Action %1").arg(i)});
    }
    QVERIFY(!model.setRootNodes(tooMany));
    QVERIFY(model.lastError().contains(QStringLiteral("nodes"), Qt::CaseInsensitive));

    ContextMenuModel::NodeSpec deep{.token = QStringLiteral("deep"),
                                    .label = QStringLiteral("Deep")};
    for (int i = 0; i < 9; ++i) {
        deep = {.kind = ContextMenuModel::NodeKind::Submenu,
                .label = QStringLiteral("Submenu %1").arg(i), .children = {deep}};
    }
    QVERIFY(!model.setRootNodes({deep}));
    QVERIFY(model.lastError().contains(QStringLiteral("depth"), Qt::CaseInsensitive));
}

void ContextMenuTest::placementFlipsAndClampsInOutputLocalCoordinates()
{
    ContextMenuPlacement::Request request;
    request.output = QRect(0, 0, 100, 80);
    request.menuSize = QSize(40, 30);
    request.anchor = QPoint(95, 75);
    request.kind = ContextMenuPlacement::Kind::Point;

    const auto desktop = ContextMenuPlacement::place(request);
    QCOMPARE(desktop.position, QPoint(55, 45));
    QVERIFY(desktop.flippedX);
    QVERIFY(desktop.flippedY);

    request.anchor = QPoint(5, 5);
    const auto topLeft = ContextMenuPlacement::place(request);
    QCOMPARE(topLeft.position, QPoint(5, 5));
    QVERIFY(!topLeft.flippedX);
    QVERIFY(!topLeft.flippedY);

    request.anchor = QPoint(95, 5);
    const auto topRight = ContextMenuPlacement::place(request);
    QCOMPARE(topRight.position, QPoint(55, 5));
    QVERIFY(topRight.flippedX);
    QVERIFY(!topRight.flippedY);

    request.anchor = QPoint(5, 75);
    const auto bottomLeft = ContextMenuPlacement::place(request);
    QCOMPARE(bottomLeft.position, QPoint(5, 45));
    QVERIFY(!bottomLeft.flippedX);
    QVERIFY(bottomLeft.flippedY);

    request.kind = ContextMenuPlacement::Kind::Dock;
    request.sourceRect = QRect(35, 65, 30, 15);
    request.anchor = {};
    const auto dock = ContextMenuPlacement::place(request);
    QCOMPARE(dock.position, QPoint(29, 35));
    QVERIFY(!dock.flippedY);

    request.kind = ContextMenuPlacement::Kind::Submenu;
    request.parentRect = QRect(70, 20, 25, 25);
    request.menuSize = QSize(50, 30);
    request.sourceRect = {};
    const auto submenu = ContextMenuPlacement::place(request);
    QCOMPARE(submenu.position, QPoint(20, 20));
    QVERIFY(submenu.flippedX);

    request.direction = Qt::RightToLeft;
    request.parentRect = QRect(5, 20, 10, 25);
    const auto rtlFallback = ContextMenuPlacement::place(request);
    QCOMPARE(rtlFallback.position, QPoint(15, 20));
    QVERIFY(rtlFallback.flippedX);

    request.direction = Qt::LeftToRight;
    request.parentRect = QRect(20, 70, 10, 5);
    const auto verticallyClamped = ContextMenuPlacement::place(request);
    QCOMPARE(verticallyClamped.position, QPoint(30, 50));

    request.output = QRect(0, 0, 20, 20);
    request.menuSize = QSize(40, 30);
    request.parentRect = {};
    request.anchor = QPoint(10, 10);
    request.kind = ContextMenuPlacement::Kind::Point;
    const auto oversized = ContextMenuPlacement::place(request);
    QCOMPARE(oversized.position, QPoint(0, 0));

    request.output = QRect(0, 0, 400, 300);
    request.menuSize = QSize(120, 80);
    request.kind = ContextMenuPlacement::Kind::CenteredRectangle;
    request.sourceRect = QRect(0, 8, 28, 28);
    request.preferredTop = 54;
    QCOMPARE(ContextMenuPlacement::place(request).position, QPoint(0, 54));

    request.sourceRect = QRect(186, 8, 28, 28);
    QCOMPARE(ContextMenuPlacement::place(request).position, QPoint(140, 54));

    request.sourceRect = QRect(372, 8, 28, 28);
    QCOMPARE(ContextMenuPlacement::place(request).position, QPoint(280, 54));

    request.output = QRect(0, 0, 96, 160);
    request.menuSize = QSize(140, 70);
    request.sourceRect = QRect(74, 8, 22, 22);
    request.preferredTop = 48;
    QCOMPARE(ContextMenuPlacement::place(request).position, QPoint(0, 48));

    request.output = QRect(0, 0, 1920, 1080);
    request.menuSize = QSize(160, 90);
    request.sourceRect = QRect(1750, 8, 28, 28);
    request.preferredTop = 54;
    QCOMPARE(ContextMenuPlacement::place(request).position, QPoint(1750 - 66, 54));
}

void ContextMenuTest::surfacePoliciesKeepInputAndLayerContracts()
{
    const auto desktop = Astrea::Shell::ContextMenuSurfacePolicy::desktopInteraction();
    QCOMPARE(desktop.layer, AstreaLayerShellConfig::Layer::Bottom);
    QCOMPARE(desktop.keyboardInteractivity, AstreaLayerShellConfig::KeyboardInteractivity::None);
    QCOMPARE(desktop.exclusiveZone, -1);
    QVERIFY(desktop.anchorTop && desktop.anchorBottom && desktop.anchorLeft && desktop.anchorRight);

    const auto overlay = Astrea::Shell::ContextMenuSurfacePolicy::overlay();
    QCOMPARE(overlay.layer, AstreaLayerShellConfig::Layer::Overlay);
    QCOMPARE(overlay.keyboardInteractivity,
             AstreaLayerShellConfig::KeyboardInteractivity::Exclusive);
    QCOMPARE(overlay.exclusiveZone, -1);
}

QTEST_MAIN(ContextMenuTest)
#include "ContextMenuTest.moc"
