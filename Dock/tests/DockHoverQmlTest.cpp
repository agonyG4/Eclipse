#include "core/DockController.hpp"

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
    void hoverModesAndTransitions();
    void reorderPreviewWorksForEveryHoverMode();
};

void DockHoverQmlTest::initTestCase()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
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
