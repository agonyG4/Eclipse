#include "core/ContextMenuController.hpp"
#include "core/ContextMenuModel.hpp"
#include "core/ContextMenuPlacement.hpp"

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
    void modelNormalizesSeparatorsAndExposesRoles();
    void modelRejectsDepthAndNodeBounds();
    void placementFlipsAndClampsInOutputLocalCoordinates();
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
}

QTEST_MAIN(ContextMenuTest)
#include "ContextMenuTest.moc"
