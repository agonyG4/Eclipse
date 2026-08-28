#include "core/ContextMenuController.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest/QtTest>

using Astrea::Shell::ContextMenuController;
using Astrea::Shell::ContextMenuAnchor;
using Astrea::Shell::ContextMenuModel;
using Astrea::Shell::ContextMenuTarget;

namespace {

QQuickItem *findContextMenuRow(QQuickItem *root, const QString &token)
{
    if (!root)
        return nullptr;
    if (root->objectName() == QStringLiteral("contextMenuRow")
        && root->property("token").toString() == token)
        return root;
    for (QQuickItem *child : root->childItems()) {
        if (QQuickItem *row = findContextMenuRow(child, token))
            return row;
    }
    return nullptr;
}

} // namespace

class ContextMenuQmlSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void sharedViewCanBeCreatedOffscreen();
    void overlayCanBeCreatedOffscreen();
    void overlayOwnsFullscreenOutputGeometryAndIntrinsicMenuSize();
    void genericMenuSizingIsContentAwareAndScrollable();
    void dockModelRowsFitViewportAndExposeLastRow();
    void desktopPlacementFlipsAndClampsToOutputEdges();
};

void ContextMenuQmlSmokeTest::sharedViewCanBeCreatedOffscreen()
{
    QQmlApplicationEngine engine;
    ContextMenuController controller;
    QVERIFY(controller.present(
        ContextMenuTarget{ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
                          QStringLiteral("test-output")},
        {ContextMenuModel::NodeSpec{.token = QStringLiteral("settings"),
                                    .label = QStringLiteral("Settings")}},
        [](const QString &) { return true; }));

    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/ContextMenu/qml/ContextMenuView.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    QObject *object = component.createWithInitialProperties({
        {QStringLiteral("menuModel"), QVariant::fromValue(controller.model())},
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("presentationGeneration"), controller.presentationGeneration()},
        {QStringLiteral("outputWidth"), 800},
        {QStringLiteral("outputHeight"), 600},
    });
    QVERIFY2(object, qPrintable(component.errorString()));
    delete object;
}

void ContextMenuQmlSmokeTest::overlayCanBeCreatedOffscreen()
{
    QQmlApplicationEngine engine;
    ContextMenuController controller;
    QVERIFY(controller.present(
        ContextMenuTarget{ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
                          QStringLiteral("test-output")},
        {ContextMenuModel::NodeSpec{.token = QStringLiteral("settings"),
                                    .label = QStringLiteral("Settings")}},
        [](const QString &) { return true; }));

    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/ContextMenu/qml/ContextMenuOverlaySurface.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    QObject *object = component.createWithInitialProperties({
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("outputKey"), QStringLiteral("test-output")},
        {QStringLiteral("outputWidth"), 800},
        {QStringLiteral("outputHeight"), 600},
    });
    QVERIFY2(object, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(object);
    QVERIFY(window);
    QVERIFY(!window->isVisible());
    delete object;
}

void ContextMenuQmlSmokeTest::overlayOwnsFullscreenOutputGeometryAndIntrinsicMenuSize()
{
    QQmlApplicationEngine engine;
    ContextMenuController controller;
    QVERIFY(controller.present(
        ContextMenuTarget{ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
                          QStringLiteral("test-output")},
        {
            ContextMenuModel::NodeSpec{.token = QStringLiteral("one"),
                                       .label = QStringLiteral("One")},
            ContextMenuModel::NodeSpec{.token = QStringLiteral("two"),
                                       .label = QStringLiteral("Two")},
            ContextMenuModel::NodeSpec{.kind = ContextMenuModel::NodeKind::Separator},
            ContextMenuModel::NodeSpec{.token = QStringLiteral("three"),
                                       .label = QStringLiteral("Three")},
        },
        [](const QString &) { return true; }));

    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/ContextMenu/qml/ContextMenuOverlaySurface.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties({
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("outputKey"), QStringLiteral("test-output")},
        {QStringLiteral("outputWidth"), 1920},
        {QStringLiteral("outputHeight"), 1080},
    }));
    QVERIFY(window);

    auto *view = window->findChild<QQuickItem *>(QStringLiteral("contextMenuView"));
    QVERIFY(view);
    auto *card = window->findChild<QQuickItem *>(QStringLiteral("contextMenuCard"));
    QVERIFY(card);
    auto *list = window->findChild<QQuickItem *>(QStringLiteral("contextMenuList"));
    QVERIFY(list);
    window->show();
    QTRY_COMPARE_WITH_TIMEOUT(window->width(), 1920, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->height(), 1080, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->contentItem()->width(), 1920.0, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->contentItem()->height(), 1080.0, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("exactContentHeight").toInt(),
                              3 * 34 + 6,
                              1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("desiredHeight").toInt(),
                              3 * 34 + 6 + 10,
                              1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("resolvedHeight").toInt(),
                              3 * 34 + 6 + 10,
                              1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("listContentHeight").toReal(),
                              3 * 34 + 6,
                              1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("cardWidth").toReal(),
                              view->property("resolvedWidth").toReal(),
                              1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("cardHeight").toReal(),
                              view->property("resolvedHeight").toReal(),
                              1000);
    QVERIFY(view->property("resolvedWidth").toReal() >= 168.0);
    QVERIFY(view->property("resolvedWidth").toReal() <= 228.0);
    QVERIFY(!view->property("scrollable").toBool());

    QVERIFY(controller.present(
        ContextMenuTarget{ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
                          QStringLiteral("test-output")},
        {ContextMenuModel::NodeSpec{.token = QStringLiteral("replacement"),
                                    .label = QStringLiteral("Replacement")}},
        [](const QString &) { return true; }));
    QTRY_COMPARE_WITH_TIMEOUT(view->property("modelRowCount").toInt(), 1, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("exactContentHeight").toInt(), 34, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("desiredHeight").toInt(), 44, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("resolvedHeight").toInt(), 44, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("listContentHeight").toReal(), 34.0, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(list->y(), 0.0, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(list->height(),
                              view->property("exactContentHeight").toReal(),
                              1000);
    QTRY_COMPARE_WITH_TIMEOUT(list->property("contentHeight").toReal(),
                              view->property("exactContentHeight").toReal(),
                              1000);
    QTRY_COMPARE_WITH_TIMEOUT(card->height(),
                              view->property("desiredHeight").toReal(),
                              1000);
    QQuickItem *replacementRow = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (replacementRow = findContextMenuRow(window->contentItem(),
                                              QStringLiteral("replacement"))) != nullptr,
        1000);
    const qreal replacementBottom = replacementRow->mapToItem(
        list, QPointF(0, replacementRow->height())).y();
    QVERIFY(replacementBottom <= list->height() + 0.01);
    QVERIFY(!view->property("scrollable").toBool());

    window->setProperty("outputWidth", 1280);
    window->setProperty("outputHeight", 720);
    QTRY_COMPARE_WITH_TIMEOUT(window->width(), 1280, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->height(), 720, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->contentItem()->width(), 1280.0, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->contentItem()->height(), 720.0, 1000);
    QTest::qWait(32);

    delete window;
}

void ContextMenuQmlSmokeTest::dockModelRowsFitViewportAndExposeLastRow()
{
    QQmlApplicationEngine engine;
    ContextMenuController controller;
    const ContextMenuTarget target{ContextMenuTarget::Kind::DockApplication,
                                   QStringLiteral("example.desktop"),
                                   QStringLiteral("test-output")};
    const QVector<ContextMenuModel::NodeSpec> nodes{
        {.token = QStringLiteral("new-window"), .label = QStringLiteral("New Window")},
        {.token = QStringLiteral("open-windows"), .label = QStringLiteral("Open Windows")},
        {.kind = ContextMenuModel::NodeKind::Separator},
        {.token = QStringLiteral("close-window"), .label = QStringLiteral("Close Window")},
        {.kind = ContextMenuModel::NodeKind::Separator},
        {.token = QStringLiteral("unpin"), .label = QStringLiteral("Unpin from Dock")},
    };
    QVERIFY(controller.present(target, nodes, [](const QString &) { return true; }));

    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/ContextMenu/qml/ContextMenuOverlaySurface.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties({
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("outputKey"), QStringLiteral("test-output")},
        {QStringLiteral("outputWidth"), 800},
        {QStringLiteral("outputHeight"), 600},
    }));
    QVERIFY(window);
    auto *view = window->findChild<QQuickItem *>(QStringLiteral("contextMenuView"));
    QVERIFY(view);
    auto *list = window->findChild<QQuickItem *>(QStringLiteral("contextMenuList"));
    QVERIFY(list);
    window->show();

    QTRY_COMPARE_WITH_TIMEOUT(view->property("modelRowCount").toInt(), 6, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(view->property("resolvedWidth").toInt() < 200, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!view->property("scrollable").toBool(), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(list->height(), list->property("contentHeight").toReal(), 1000);

    QQuickItem *lastRow = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((lastRow = findContextMenuRow(window->contentItem(),
                                                           QStringLiteral("unpin"))) != nullptr,
                             1000);
    const qreal lastRowBottom = lastRow->mapToItem(
        list, QPointF(0, lastRow->height())).y();
    QVERIFY(lastRowBottom <= list->height() + 0.01);

    delete window;
}

void ContextMenuQmlSmokeTest::genericMenuSizingIsContentAwareAndScrollable()
{
    QQmlApplicationEngine engine;
    ContextMenuController controller;
    const QString longLabel = QStringLiteral(
        "A deliberately long context menu label that must be measured and clamped to the maximum width");

    QVERIFY(controller.present(
        ContextMenuTarget{ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
                          QStringLiteral("test-output")},
        {ContextMenuModel::NodeSpec{.token = QStringLiteral("long"), .label = longLabel}},
        [](const QString &) { return true; }));

    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/ContextMenu/qml/ContextMenuOverlaySurface.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties({
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("outputKey"), QStringLiteral("test-output")},
        {QStringLiteral("outputWidth"), 800},
        {QStringLiteral("outputHeight"), 600},
    }));
    QVERIFY(window);

    auto *view = window->findChild<QQuickItem *>(QStringLiteral("contextMenuView"));
    QVERIFY(view);
    window->show();
    QTRY_COMPARE_WITH_TIMEOUT(view->property("naturalWidth").toInt() > 228, true, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("resolvedWidth").toInt(), 228, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("exactContentHeight").toInt(), 34, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("resolvedHeight").toInt(), 44, 1000);

    window->setProperty("outputHeight", 160);
    QTRY_COMPARE_WITH_TIMEOUT(window->height(), 160, 1000);

    QVector<ContextMenuModel::NodeSpec> tallRows;
    for (int index = 0; index < 12; ++index) {
        tallRows.append(ContextMenuModel::NodeSpec{
            .token = QStringLiteral("row-%1").arg(index),
            .label = QStringLiteral("Row %1").arg(index),
        });
    }
    QVERIFY(controller.present(
        ContextMenuTarget{ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
                          QStringLiteral("test-output")},
        tallRows,
        [](const QString &) { return true; }));

    QTRY_COMPARE_WITH_TIMEOUT(view->property("modelRowCount").toInt(), 12, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("exactContentHeight").toInt(), 12 * 34, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("desiredHeight").toInt(), 12 * 34 + 10, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("resolvedHeight").toInt(), 160 - 2 * 8, 1000);
    QVERIFY(view->property("scrollable").toBool());
    QTRY_COMPARE_WITH_TIMEOUT(view->property("cardHeight").toReal(),
                              view->property("resolvedHeight").toReal(),
                              1000);

    delete window;
}

void ContextMenuQmlSmokeTest::desktopPlacementFlipsAndClampsToOutputEdges()
{
    QQmlApplicationEngine engine;
    ContextMenuController controller;
    const ContextMenuTarget target{ContextMenuTarget::Kind::Desktop,
                                   QStringLiteral("desktop"), QStringLiteral("test-output")};
    const QVector<ContextMenuModel::NodeSpec> nodes{
        {.token = QStringLiteral("settings"), .label = QStringLiteral("Settings")},
    };
    ContextMenuAnchor anchor;
    anchor.kind = ContextMenuAnchor::Kind::Point;
    anchor.point = QPoint(0, 0);
    QVERIFY(controller.present(target, anchor, nodes, [](const QString &) { return true; }));

    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/ContextMenu/qml/ContextMenuOverlaySurface.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties({
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("outputKey"), QStringLiteral("test-output")},
        {QStringLiteral("outputWidth"), 800},
        {QStringLiteral("outputHeight"), 600},
    }));
    QVERIFY(window);
    auto *view = window->findChild<QQuickItem *>(QStringLiteral("contextMenuView"));
    QVERIFY(view);
    window->show();
    QTRY_COMPARE_WITH_TIMEOUT(view->property("resolvedWidth").toInt(), 168, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->x(), 8.0, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->y(), 8.0, 1000);

    anchor.point = QPoint(799, 599);
    QVERIFY(controller.present(target, anchor, nodes, [](const QString &) { return true; }));
    QTRY_COMPARE_WITH_TIMEOUT(view->property("resolvedWidth").toInt(), 168, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("resolvedHeight").toInt(), 44, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->x(), 800 - 168 - 8, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->y(), 600 - 44 - 8, 1000);

    delete window;
}

QTEST_MAIN(ContextMenuQmlSmokeTest)
#include "ContextMenuQmlSmokeTest.moc"
