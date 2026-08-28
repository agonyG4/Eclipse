#include "core/ContextMenuController.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest/QtTest>

using Astrea::Shell::ContextMenuController;
using Astrea::Shell::ContextMenuModel;
using Astrea::Shell::ContextMenuTarget;

class ContextMenuQmlSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void sharedViewCanBeCreatedOffscreen();
    void overlayCanBeCreatedOffscreen();
    void overlayOwnsFullscreenOutputGeometryAndIntrinsicMenuSize();
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
    window->show();
    QTRY_COMPARE_WITH_TIMEOUT(window->width(), 1920, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->height(), 1080, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->contentItem()->width(), 1920.0, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->contentItem()->height(), 1080.0, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->width(), 280.0, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(view->property("listContentHeight").toReal()
                                 >= 3 * 36 + 10,
                             1000);
    QTRY_VERIFY_WITH_TIMEOUT(view->property("implicitHeight").toReal()
                                 >= 3 * 36 + 10 + 20,
                             1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("cardWidth").toReal(), 280.0, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("cardHeight").toReal(),
                              view->property("implicitHeight").toReal(), 1000);

    QVERIFY(controller.present(
        ContextMenuTarget{ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
                          QStringLiteral("test-output")},
        {ContextMenuModel::NodeSpec{.token = QStringLiteral("replacement"),
                                    .label = QStringLiteral("Replacement")}},
        [](const QString &) { return true; }));
    QTRY_COMPARE_WITH_TIMEOUT(view->property("modelRowCount").toInt(), 1, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("listContentHeight").toReal(), 36.0, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("implicitHeight").toReal(), 56.0, 1000);

    window->setProperty("outputWidth", 1280);
    window->setProperty("outputHeight", 720);
    QTRY_COMPARE_WITH_TIMEOUT(window->width(), 1280, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->height(), 720, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->contentItem()->width(), 1280.0, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->contentItem()->height(), 720.0, 1000);
    QTest::qWait(32);

    delete window;
}

QTEST_MAIN(ContextMenuQmlSmokeTest)
#include "ContextMenuQmlSmokeTest.moc"
