#include "core/ContextMenuController.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
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

QTEST_MAIN(ContextMenuQmlSmokeTest)
#include "ContextMenuQmlSmokeTest.moc"
