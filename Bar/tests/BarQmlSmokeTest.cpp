#include "core/BarClockService.hpp"
#include "core/BarLayoutMetrics.hpp"
#include "core/BarPopupController.hpp"
#include "theme/ThemeController.hpp"

#include <QGuiApplication>
#include <QCoreApplication>
#include <QColor>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTest>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantMap>

class BarQmlSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsAllProductionSurfaces();
    void statusSurfaceUsesProductionGeometryAuthority();
    void popupSurfaceUsesProductionClampAndClosingLifecycle();
    void clockUsesReferenceHorizontalStructure();
    void barPaletteFollowsSharedThemeState();

private:
    void loadsProductionSurface(const QString &fileName);
};

void BarQmlSmokeTest::loadsProductionSurface(const QString &fileName)
{
    QQmlEngine engine;
    const QUrl url(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Bar/qml/") + fileName);
    QQmlComponent component(&engine, url);
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errors().isEmpty()
                 ? QStringLiteral("Bar QML component is not ready")
                 : component.errors().constFirst().toString()));
    QVariantMap properties;
    QObject *object = component.createWithInitialProperties(properties);
    QVERIFY2(object, qPrintable(component.errors().isEmpty()
        ? QStringLiteral("Bar QML component did not instantiate")
        : component.errors().constFirst().toString()));
    QVERIFY(qobject_cast<QQuickWindow *>(object));
    if (fileName == QStringLiteral("LauncherSurface.qml")) {
        QObject *logo = object->findChild<QObject *>(QStringLiteral("logoImage"));
        QVERIFY(logo != nullptr);
        QCOMPARE(logo->property("source").toUrl(),
                 QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Bar/assets/astrea.png")));
        QTRY_COMPARE_WITH_TIMEOUT(logo->property("status").toInt(), 1, 1000);
    }
    delete object;
}

void BarQmlSmokeTest::loadsAllProductionSurfaces()
{
    const QStringList files{
        QStringLiteral("ReserveSurface.qml"),
        QStringLiteral("LauncherSurface.qml"),
        QStringLiteral("StatusSurface.qml"),
        QStringLiteral("PopupOverlaySurface.qml"),
    };
    for (const QString &file : files)
        loadsProductionSurface(file);
}

void BarQmlSmokeTest::statusSurfaceUsesProductionGeometryAuthority()
{
    QQmlEngine engine;
    BarLayoutMetrics metrics;
    BarClockService clock;
    clock.start();
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/Bar/qml/StatusSurface.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errors().isEmpty()
                 ? QStringLiteral("Status surface is not ready")
                 : component.errors().constFirst().toString()));
    QObject *status = component.createWithInitialProperties({
        {QStringLiteral("barGeometry"), QVariant::fromValue(&metrics)},
        {QStringLiteral("clockService"), QVariant::fromValue(&clock)},
        {QStringLiteral("outputWidth"), 800},
        {QStringLiteral("launcherWidth"), 100},
    });
    QVERIFY(status != nullptr);
    QCoreApplication::processEvents();
    QObject *pill = status->findChild<QObject *>(QStringLiteral("statusPill"));
    QVERIFY(pill != nullptr);
    const int pillWidth = qRound(pill->property("implicitWidth").toReal());
    QCOMPARE(status->property("width").toInt(), metrics.statusWidth(800, 100, pillWidth));
    QCOMPARE(status->property("clockAnchorX").toInt(),
             metrics.statusAnchorX(800, status->property("width").toInt(),
                                   qRound(status->property("clockIndicatorLocalX").toReal())));
    delete status;
}

void BarQmlSmokeTest::popupSurfaceUsesProductionClampAndClosingLifecycle()
{
    QQmlEngine engine;
    BarLayoutMetrics metrics;
    BarPopupController popup;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/Bar/qml/PopupOverlaySurface.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errors().isEmpty()
                 ? QStringLiteral("Popup surface is not ready")
                 : component.errors().constFirst().toString()));
    QObject *overlay = component.createWithInitialProperties({
        {QStringLiteral("barGeometry"), QVariant::fromValue(&metrics)},
        {QStringLiteral("popupController"), QVariant::fromValue(&popup)},
        {QStringLiteral("outputWidth"), 100},
        {QStringLiteral("outputHeight"), 300},
    });
    QVERIFY(overlay != nullptr);
    QObject *menu = overlay->findChild<QObject *>(QStringLiteral("astreaMenu"));
    QVERIFY(menu != nullptr);

    popup.open(BarPopupController::PopupKind::AstreaMenu, 96);
    QCoreApplication::processEvents();
    QCOMPARE(menu->property("width").toInt(), metrics.popupWidth(100, 200));
    QCOMPARE(menu->property("x").toInt(), metrics.popupX(100, menu->property("width").toInt(), 96));

    popup.close();
    QCoreApplication::processEvents();
    QVERIFY(popup.closing());
    QVERIFY(popup.surfaceRequired());
    QVERIFY(menu->property("visible").toBool());
    QTest::qWait(260);
    QVERIFY(!popup.surfaceRequired());
    QVERIFY(!menu->property("visible").toBool());
    delete overlay;
}

void BarQmlSmokeTest::clockUsesReferenceHorizontalStructure()
{
    QQmlEngine engine;
    BarClockService clock;
    clock.start();
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/Bar/qml/StatusSurface.qml")));
    QObject *status = component.createWithInitialProperties({
        {QStringLiteral("barGeometry"), QVariant::fromValue(new BarLayoutMetrics(&engine))},
        {QStringLiteral("clockService"), QVariant::fromValue(&clock)},
        {QStringLiteral("outputWidth"), 800},
        {QStringLiteral("launcherWidth"), 80},
    });
    QVERIFY(status != nullptr);
    QObject *clockObject = status->findChild<QObject *>(QStringLiteral("clock"));
    QObject *date = status->findChild<QObject *>(QStringLiteral("clockDate"));
    QObject *separator = status->findChild<QObject *>(QStringLiteral("clockSeparator"));
    QObject *time = status->findChild<QObject *>(QStringLiteral("clockTime"));
    QVERIFY(clockObject != nullptr);
    QVERIFY(date != nullptr);
    QVERIFY(separator != nullptr);
    QVERIFY(time != nullptr);
    QCOMPARE(separator->property("width").toInt(), 1);
    QVERIFY(separator->property("height").toInt() > separator->property("width").toInt());
    QCOMPARE(date->property("y").toInt(), time->property("y").toInt());
    QVERIFY(clockObject->property("implicitWidth").toInt()
            > clockObject->property("implicitHeight").toInt());
    delete status;
}

void BarQmlSmokeTest::barPaletteFollowsSharedThemeState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ThemeController controller(directory.filePath(QStringLiteral("missing-theme.json")));
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &controller);
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/Bar/qml/components/ShellBarTheme.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errors().isEmpty()
                 ? QStringLiteral("ShellBarTheme is not ready")
                 : component.errors().constFirst().toString()));
    QObject *theme = component.create();
    QVERIFY(theme != nullptr);

    const QColor darkDefault = theme->property("shellTextMain").value<QColor>();
    const QColor darkSurface = theme->property("shellSurface").value<QColor>();
    QVERIFY(!theme->property("isLight").toBool());
    QVERIFY(theme->property("isTransparent").toBool());

    controller.setThemeMode(1);
    QCoreApplication::processEvents();
    QVERIFY(theme->property("isLight").toBool());
    QVERIFY(theme->property("shellTextMain").value<QColor>() != darkDefault);

    controller.setShellStyle(2);
    QCoreApplication::processEvents();
    QVERIFY(theme->property("isFrosted").toBool());
    QVERIFY(theme->property("shellSurface").value<QColor>() != darkSurface);

    controller.setThemeMode(0);
    controller.setAccentHex(QStringLiteral("#30d158"));
    QCoreApplication::processEvents();
    QCOMPARE(theme->property("shellIconAccent").value<QColor>(), QColor(QStringLiteral("#30d158")));
    delete theme;
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication application(argc, argv);
    BarQmlSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "BarQmlSmokeTest.moc"
