#include "core/BarClockService.hpp"
#include "core/BarLayoutMetrics.hpp"
#include "core/BarPopupController.hpp"
#include "theme/ThemeController.hpp"

#include <QGuiApplication>
#include <QCoreApplication>
#include <QColor>
#include <QFont>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantMap>

class BarQmlSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsAllProductionSurfaces();
    void barPaletteMatchesBorealisForAllSixCombinations();
    void barSegmentUsesBorealisInteractionTokens();
    void statusSurfaceUsesProductionGeometryAuthority();
    void popupSurfaceUsesProductionClampAndClosingLifecycle();
    void popupEntersAndCompletesAnimation();
    void popupReopenCancelsExitAnimation();
    void popupKindSwitchCancelsPreviousExit();
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

void BarQmlSmokeTest::barPaletteMatchesBorealisForAllSixCombinations()
{
    struct ExpectedPalette {
        int themeMode;
        int shellStyle;
        QColor background;
        QColor surface;
        QColor border;
        QColor borderHover;
        QColor hover;
        QColor textPrimary;
        QColor textSecondary;
        QColor separator;
    };
    const auto rgba = [](qreal red, qreal green, qreal blue, qreal alpha) {
        return QColor::fromRgbF(red, green, blue, alpha);
    };
    const QColor darkTextMain(QStringLiteral("#f5f5f7"));
    const QColor darkTextSecondary = rgba(1, 1, 1, 0.60);
    const QColor darkHover = rgba(1, 1, 1, 0.08);
    const QColor darkBorderHover = rgba(1, 1, 1, 0.28);
    const QColor darkSeparator = rgba(1, 1, 1, 0.08);
    const QColor lightTextMain = rgba(0.05, 0.06, 0.07, 0.94);
    const QColor lightTextSecondary = rgba(0.13, 0.15, 0.18, 0.68);
    const QColor lightHover = rgba(0, 0, 0, 0.055);
    const QColor lightBorderHover = rgba(0, 0, 0, 0.20);
    const QColor lightSeparator = rgba(1, 1, 1, 0.08);
    const QList<ExpectedPalette> expected{
        {0, 0, rgba(0, 0, 0, 0.06), rgba(1, 1, 1, 0.06),
         rgba(1, 1, 1, 0.14), darkBorderHover, darkHover, darkTextMain,
         darkTextSecondary, darkSeparator},
        {0, 1, rgba(0.10, 0.10, 0.11, 0.96), rgba(1, 1, 1, 0.08),
         rgba(1, 1, 1, 0.11), darkBorderHover, darkHover, darkTextMain,
         darkTextSecondary, darkSeparator},
        {0, 2, rgba(0, 0, 0, 0.06), rgba(1, 1, 1, 0.06),
         rgba(1, 1, 1, 0.14), darkBorderHover, darkHover, darkTextMain,
         darkTextSecondary, darkSeparator},
        {1, 0, rgba(1, 1, 1, 0.16), rgba(1, 1, 1, 0.22),
         rgba(0, 0, 0, 0.08), lightBorderHover, lightHover, lightTextMain,
         lightTextSecondary, lightSeparator},
        {1, 1, rgba(0.985, 0.987, 0.994, 0.92), rgba(1, 1, 1, 0.86),
         rgba(0, 0, 0, 0.12), lightBorderHover, lightHover, lightTextMain,
         lightTextSecondary, lightSeparator},
        {1, 2, rgba(0.96, 0.985, 1, 0.30), rgba(0.98, 0.99, 1, 0.38),
         rgba(0, 0, 0, 0.10), lightBorderHover, lightHover, lightTextMain,
         lightTextSecondary, lightSeparator},
    };

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ThemeController controller(directory.filePath(QStringLiteral("missing-theme.json")));
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &controller);
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/Bar/qml/components/ShellBarTheme.qml")));
    QVERIFY(component.status() == QQmlComponent::Ready);
    QObject *theme = component.create();
    QVERIFY(theme != nullptr);

    for (const ExpectedPalette &values : expected) {
        controller.setThemeMode(values.themeMode);
        controller.setShellStyle(values.shellStyle);
        QCoreApplication::processEvents();
        QCOMPARE(theme->property("shellBackground").value<QColor>(), values.background);
        QCOMPARE(theme->property("shellSurface").value<QColor>(), values.surface);
        QCOMPARE(theme->property("shellBorder").value<QColor>(), values.border);
        QCOMPARE(theme->property("shellBorderHover").value<QColor>(), values.borderHover);
        QCOMPARE(theme->property("shellHover").value<QColor>(), values.hover);
        QCOMPARE(theme->property("shellTextMain").value<QColor>(), values.textPrimary);
        QCOMPARE(theme->property("shellTextSecondary").value<QColor>(), values.textSecondary);
        QCOMPARE(theme->property("shellSeparator").value<QColor>(), values.separator);
    }
    delete theme;
}

void BarQmlSmokeTest::barSegmentUsesBorealisInteractionTokens()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ThemeController controller(directory.filePath(QStringLiteral("missing-theme.json")));
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &controller);
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/Bar/qml/components/BarSegment.qml")));
    QVERIFY(component.status() == QQmlComponent::Ready);
    QObject *object = component.create();
    auto *segment = qobject_cast<QQuickItem *>(object);
    QVERIFY(segment != nullptr);
    QObject *surface = object->findChild<QObject *>(QStringLiteral("barSegmentSurface"));
    QVERIFY(surface != nullptr);

    const auto color = [surface] { return surface->property("color").value<QColor>(); };
    const auto borderColor = [surface] {
        return QQmlProperty(surface, QStringLiteral("border.color")).read().value<QColor>();
    };
    QCOMPARE(color(), QColor::fromRgbF(0, 0, 0, 0.06));
    QCOMPARE(borderColor(), QColor::fromRgbF(1, 1, 1, 0.14));

    QQuickWindow window;
    window.resize(120, 50);
    segment->setParentItem(window.contentItem());
    segment->setProperty("interactive", true);
    segment->setWidth(80);
    segment->setHeight(36);
    window.show();
    QTest::qWait(20);

    QTest::mouseMove(&window, QPoint(30, 25));
    QCoreApplication::processEvents();
    QTRY_COMPARE_WITH_TIMEOUT(color(), QColor::fromRgbF(1, 1, 1, 0.08), 500);
    QTRY_COMPARE_WITH_TIMEOUT(borderColor(), QColor::fromRgbF(1, 1, 1, 0.28), 500);

    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, QPoint(30, 25));
    QCoreApplication::processEvents();
    QTRY_COMPARE_WITH_TIMEOUT(color(), QColor::fromRgbF(1, 1, 1, 0.12), 500);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, QPoint(30, 25));

    segment->setProperty("active", true);
    QCoreApplication::processEvents();
    QTRY_COMPARE_WITH_TIMEOUT(color(), QColor::fromRgbF(1, 1, 1, 0.15), 500);
    delete object;
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

void BarQmlSmokeTest::popupEntersAndCompletesAnimation()
{
    QQmlEngine engine;
    BarLayoutMetrics metrics;
    BarPopupController popup;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/Bar/qml/PopupOverlaySurface.qml")));
    QObject *overlay = component.createWithInitialProperties({
        {QStringLiteral("barGeometry"), QVariant::fromValue(&metrics)},
        {QStringLiteral("popupController"), QVariant::fromValue(&popup)},
        {QStringLiteral("outputWidth"), 1200},
        {QStringLiteral("outputHeight"), 700},
    });
    QVERIFY(overlay != nullptr);
    QObject *menu = overlay->findChild<QObject *>(QStringLiteral("astreaMenu"));
    QVERIFY(menu != nullptr);

    popup.open(BarPopupController::PopupKind::AstreaMenu, 600);
    QCoreApplication::processEvents();
    QVERIFY(menu->property("visible").toBool());
    QVERIFY(menu->property("opacity").toReal() < 1.0);
    QVERIFY(menu->property("scale").toReal() < 1.0);
    QTRY_VERIFY_WITH_TIMEOUT(menu->property("opacity").toReal() > 0.99, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(menu->property("scale").toReal() > 0.99, 1000);
    QVERIFY(popup.isOpen());
    delete overlay;
}

void BarQmlSmokeTest::popupReopenCancelsExitAnimation()
{
    QQmlEngine engine;
    BarLayoutMetrics metrics;
    BarPopupController popup;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/Bar/qml/PopupOverlaySurface.qml")));
    QObject *overlay = component.createWithInitialProperties({
        {QStringLiteral("barGeometry"), QVariant::fromValue(&metrics)},
        {QStringLiteral("popupController"), QVariant::fromValue(&popup)},
    });
    QVERIFY(overlay != nullptr);
    popup.open(BarPopupController::PopupKind::Clock, 500);
    QTest::qWait(40);
    popup.close();
    QCoreApplication::processEvents();
    QVERIFY(popup.closing());
    popup.open(BarPopupController::PopupKind::Clock, 520);
    QCoreApplication::processEvents();
    QVERIFY(popup.isOpen());
    QVERIFY(!popup.closing());
    QTest::qWait(300);
    QVERIFY(popup.surfaceRequired());
    QCOMPARE(popup.kind(), BarPopupController::PopupKind::Clock);
    QCOMPARE(popup.anchorX(), 520);
    delete overlay;
}

void BarQmlSmokeTest::popupKindSwitchCancelsPreviousExit()
{
    QQmlEngine engine;
    BarLayoutMetrics metrics;
    BarPopupController popup;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/Bar/qml/PopupOverlaySurface.qml")));
    QObject *overlay = component.createWithInitialProperties({
        {QStringLiteral("barGeometry"), QVariant::fromValue(&metrics)},
        {QStringLiteral("popupController"), QVariant::fromValue(&popup)},
    });
    QVERIFY(overlay != nullptr);
    popup.open(BarPopupController::PopupKind::Clock, 500);
    QTest::qWait(40);
    popup.close();
    QCoreApplication::processEvents();
    popup.open(BarPopupController::PopupKind::AstreaMenu, 120);
    QCoreApplication::processEvents();
    QTest::qWait(300);
    QVERIFY(popup.surfaceRequired());
    QVERIFY(popup.isOpen());
    QCOMPARE(popup.kind(), BarPopupController::PopupKind::AstreaMenu);
    QCOMPARE(popup.anchorX(), 120);
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
    QObject *row = status->findChild<QObject *>(QStringLiteral("clockRow"));
    QVERIFY(clockObject != nullptr);
    QVERIFY(date != nullptr);
    QVERIFY(separator != nullptr);
    QVERIFY(time != nullptr);
    QVERIFY(row != nullptr);
    QCOMPARE(separator->property("width").toInt(), 1);
    QVERIFY(separator->property("height").toInt() > separator->property("width").toInt());
    QCOMPARE(date->property("y").toInt(), time->property("y").toInt());
    QCOMPARE(row->property("spacing").toInt(), 8);
    QCOMPARE(QQmlProperty(date, QStringLiteral("font.family")).read().toString(),
             QStringLiteral("Inter"));
    QCOMPARE(QQmlProperty(date, QStringLiteral("font.weight")).read().toInt(),
             static_cast<int>(QFont::Normal));
    QCOMPARE(QQmlProperty(time, QStringLiteral("font.weight")).read().toInt(),
             static_cast<int>(QFont::Medium));
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
