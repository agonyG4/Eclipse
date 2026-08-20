#include "core/BarClockService.hpp"
#include "core/BarLayoutMetrics.hpp"
#include "core/BarPopupController.hpp"
#include "core/WorkspaceModel.hpp"
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

class FakeAudioService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged)

public:
    explicit FakeAudioService(QObject *parent = nullptr) : QObject(parent) {}
    double volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    bool available() const { return true; }
    Q_INVOKABLE void adjustVolume(double delta) {
        m_lastDelta = delta;
        m_volume += delta;
        emit volumeChanged();
    }
    Q_INVOKABLE void setMuted(bool muted) {
        m_muted = muted;
        emit mutedChanged();
    }
    void setVolumeForTest(double volume) {
        m_volume = volume;
        emit volumeChanged();
    }
    double m_lastDelta = 0.0;

signals:
    void volumeChanged();
    void mutedChanged();

private:
    double m_volume = 50.0;
    bool m_muted = false;
};

class FakeNetworkService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool wifiAvailable READ wifiAvailable NOTIFY changed)
    Q_PROPERTY(int connectionType READ connectionType NOTIFY changed)
    Q_PROPERTY(QString connectionName READ connectionName NOTIFY changed)

public:
    explicit FakeNetworkService(QObject *parent = nullptr) : QObject(parent) {}
    bool connected() const { return m_connected; }
    bool wifiAvailable() const { return m_wifiAvailable; }
    int connectionType() const { return m_connectionType; }
    QString connectionName() const { return m_connectionName; }

    void setState(bool connected, bool wifiAvailable, int connectionType,
                  const QString &connectionName)
    {
        m_connected = connected;
        m_wifiAvailable = wifiAvailable;
        m_connectionType = connectionType;
        m_connectionName = connectionName;
        emit changed();
    }

signals:
    void changed();

private:
    bool m_connected = true;
    bool m_wifiAvailable = true;
    int m_connectionType = 1;
    QString m_connectionName = QStringLiteral("Office");
};

class FakeBluetoothService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool adapterAvailable READ adapterAvailable NOTIFY changed)
    Q_PROPERTY(bool powered READ powered NOTIFY changed)
    Q_PROPERTY(int connectedCount READ connectedCount NOTIFY changed)
    Q_PROPERTY(bool scanning READ scanning NOTIFY changed)

public:
    explicit FakeBluetoothService(QObject *parent = nullptr) : QObject(parent) {}
    bool available() const { return m_available; }
    bool powered() const { return m_powered; }
    bool adapterAvailable() const { return m_adapterAvailable; }
    int connectedCount() const { return m_connectedCount; }
    bool scanning() const { return m_scanning; }

    void setState(bool available, bool adapterAvailable, bool powered,
                  int connectedCount, bool scanning)
    {
        m_available = available;
        m_adapterAvailable = adapterAvailable;
        m_powered = powered;
        m_connectedCount = connectedCount;
        m_scanning = scanning;
        emit changed();
    }

signals:
    void changed();

private:
    bool m_available = true;
    bool m_adapterAvailable = true;
    bool m_powered = true;
    int m_connectedCount = 1;
    bool m_scanning = true;
};

class BarQmlSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsAllProductionSurfaces();
    void barPaletteMatchesBorealisForAllSixCombinations();
    void barSegmentUsesBorealisInteractionTokens();
    void statusSurfaceUsesProductionGeometryAuthority();
    void statusSurfaceUsesInjectedSystemServices();
    void popupSurfaceUsesProductionClampAndClosingLifecycle();
    void popupEntersAndCompletesAnimation();
    void popupReopenCancelsExitAnimation();
    void popupKindSwitchCancelsPreviousExit();
    void clockUsesReferenceHorizontalStructure();
    void barPaletteFollowsSharedThemeState();
    void launcherAndStatusUsePerIndicatorHitTargets();
    void indicatorGlyphsMatchReferenceStates();
    void workspaceDelegatesExposeReferenceHitboxes();
    void popupVisualsUseNativeServiceInputs();

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
         lightTextSecondary, rgba(0, 0, 0, 0.065)},
        {1, 1, rgba(0.985, 0.987, 0.994, 0.92), rgba(1, 1, 1, 0.86),
         rgba(0, 0, 0, 0.12), lightBorderHover, lightHover, lightTextMain,
         lightTextSecondary, rgba(0, 0, 0, 0.055)},
        {1, 2, rgba(0.96, 0.985, 1, 0.30), rgba(0.98, 0.99, 1, 0.38),
         rgba(0, 0, 0, 0.10), lightBorderHover, lightHover, lightTextMain,
         lightTextSecondary, rgba(0, 0, 0, 0.065)},
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

void BarQmlSmokeTest::statusSurfaceUsesInjectedSystemServices()
{
    QQmlEngine engine;
    BarLayoutMetrics metrics;
    BarClockService clock;
    clock.start();
    FakeAudioService audio;
    FakeNetworkService network;
    FakeBluetoothService bluetooth;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/Bar/qml/StatusSurface.qml")));
    QObject *status = component.createWithInitialProperties({
        {QStringLiteral("barGeometry"), QVariant::fromValue(&metrics)},
        {QStringLiteral("clockService"), QVariant::fromValue(&clock)},
        {QStringLiteral("audioService"), QVariant::fromValue(&audio)},
        {QStringLiteral("networkService"), QVariant::fromValue(&network)},
        {QStringLiteral("bluetoothService"), QVariant::fromValue(&bluetooth)},
        {QStringLiteral("outputWidth"), 800},
        {QStringLiteral("launcherWidth"), 100},
    });
    QVERIFY(status != nullptr);
    auto *window = qobject_cast<QQuickWindow *>(status);
    QVERIFY(window != nullptr);
    QObject *networkIndicator = status->findChild<QObject *>(
        QStringLiteral("networkIndicator"));
    QObject *bluetoothIndicator = status->findChild<QObject *>(
        QStringLiteral("bluetoothIndicator"));
    QObject *volumeIndicator = status->findChild<QObject *>(
        QStringLiteral("volumeIndicator"));
    QVERIFY(networkIndicator != nullptr);
    QVERIFY(bluetoothIndicator != nullptr);
    QVERIFY(volumeIndicator != nullptr);
    QCOMPARE(networkIndicator->property("networkService").value<QObject *>(), &network);
    QCOMPARE(bluetoothIndicator->property("bluetoothService").value<QObject *>(), &bluetooth);
    QCOMPARE(volumeIndicator->property("audioService").value<QObject *>(), &audio);
    QCOMPARE(networkIndicator->findChild<QObject *>(QStringLiteral("networkIcon"))
                 ->property("text").toString(), QStringLiteral("󰖩"));
    QCOMPARE(bluetoothIndicator->findChild<QObject *>(QStringLiteral("bluetoothIcon"))
                 ->property("text").toString(), QStringLiteral("󰂯"));
    QCOMPARE(volumeIndicator->findChild<QObject *>(QStringLiteral("volumeIcon"))
                 ->property("text").toString(), QStringLiteral("󰖀"));
    QObject *volumeWheel = volumeIndicator->findChild<QObject *>(
        QStringLiteral("volumeWheelArea"));
    auto *wheelItem = qobject_cast<QQuickItem *>(volumeWheel);
    QVERIFY(wheelItem != nullptr);
    window->resize(800, 36);
    window->show();
    QTest::qWait(20);
    const QPointF position = wheelItem->mapToItem(window->contentItem(),
                                                   QPointF(wheelItem->width() / 2,
                                                            wheelItem->height() / 2));
    QTest::wheelEvent(window, position, QPoint(0, 120));
    QCoreApplication::processEvents();
    QCOMPARE(audio.m_lastDelta, 2.0);
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
    QCOMPARE(row->property("spacing").toInt(), 0);
    QCOMPARE(QQmlProperty(date, QStringLiteral("font.family")).read().toString(),
             QStringLiteral("Inter Display"));
    QCOMPARE(QQmlProperty(date, QStringLiteral("font.weight")).read().toInt(),
             static_cast<int>(QFont::Medium));
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

void BarQmlSmokeTest::launcherAndStatusUsePerIndicatorHitTargets()
{
    QQmlEngine engine;
    BarLayoutMetrics metrics;
    BarPopupController popup;
    WorkspaceModel workspaceModel;
    workspaceModel.replaceWorkspaces({
        {QStringLiteral("1"), true, true, false, {}},
        {QStringLiteral("2"), false, false, false, {}},
    });

    QQmlComponent launcherComponent(
        &engine, QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Bar/qml/LauncherSurface.qml")));
    QObject *launcher = launcherComponent.createWithInitialProperties({
        {QStringLiteral("barGeometry"), QVariant::fromValue(&metrics)},
        {QStringLiteral("popupController"), QVariant::fromValue(&popup)},
        {QStringLiteral("workspaceModel"), QVariant::fromValue(&workspaceModel)},
    });
    QVERIFY(launcher != nullptr);
    QObject *launcherPill = launcher->findChild<QObject *>(QStringLiteral("launcherPill"));
    QObject *logoButton = launcher->findChild<QObject *>(QStringLiteral("logoButton"));
    QVERIFY(launcherPill != nullptr);
    QVERIFY(logoButton != nullptr);
    QVERIFY(!launcherPill->property("interactive").toBool());
    QVERIFY(logoButton->property("interactive").toBool());
    QCOMPARE(logoButton->property("fixedWidth").toInt(), 28);

    QQmlComponent statusComponent(
        &engine, QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Bar/qml/StatusSurface.qml")));
    QObject *status = statusComponent.createWithInitialProperties({
        {QStringLiteral("barGeometry"), QVariant::fromValue(&metrics)},
        {QStringLiteral("popupController"), QVariant::fromValue(&popup)},
        {QStringLiteral("workspaceModel"), QVariant::fromValue(&workspaceModel)},
        {QStringLiteral("outputWidth"), 1200},
        {QStringLiteral("launcherWidth"), 120},
    });
    QVERIFY(status != nullptr);
    QObject *statusPill = status->findChild<QObject *>(QStringLiteral("statusPill"));
    QVERIFY(statusPill != nullptr);
    QVERIFY(!statusPill->property("interactive").toBool());
    for (const QString &name : {QStringLiteral("networkIndicator"),
                                QStringLiteral("bluetoothIndicator"),
                                QStringLiteral("volumeIndicator"),
                                QStringLiteral("clock")}) {
        QObject *indicator = status->findChild<QObject *>(name);
        QVERIFY2(indicator != nullptr, qPrintable(name));
        QVERIFY2(indicator->property("interactive").toBool(), qPrintable(name));
    }

    delete status;
    delete launcher;
}

void BarQmlSmokeTest::indicatorGlyphsMatchReferenceStates()
{
    QQmlEngine engine;
    FakeAudioService audio;
    FakeNetworkService network;
    FakeBluetoothService bluetooth;

    QQmlComponent networkComponent(
        &engine, QUrl(QStringLiteral(
            "qrc:/qt/qml/Astrea/Shell/Bar/qml/components/NetworkIndicator.qml")));
    QObject *networkIndicator = networkComponent.createWithInitialProperties({
        {QStringLiteral("networkService"), QVariant::fromValue(&network)},
    });
    QVERIFY(networkIndicator != nullptr);
    QObject *networkIcon = networkIndicator->findChild<QObject *>(QStringLiteral("networkIcon"));
    QVERIFY(networkIcon != nullptr);
    QCOMPARE(networkIcon->property("text").toString(), QStringLiteral("󰖩"));
    QVERIFY(networkIndicator->findChild<QObject *>(QStringLiteral("networkLabel")) == nullptr);
    network.setState(false, false, 0, {});
    QCoreApplication::processEvents();
    QCOMPARE(networkIcon->property("text").toString(), QStringLiteral("󰖪"));
    network.setState(true, true, 2, QStringLiteral("Ethernet"));
    QCoreApplication::processEvents();
    QCOMPARE(networkIcon->property("text").toString(), QStringLiteral("󰈀"));

    QQmlComponent bluetoothComponent(
        &engine, QUrl(QStringLiteral(
            "qrc:/qt/qml/Astrea/Shell/Bar/qml/components/BluetoothIndicator.qml")));
    QObject *bluetoothIndicator = bluetoothComponent.createWithInitialProperties({
        {QStringLiteral("bluetoothService"), QVariant::fromValue(&bluetooth)},
    });
    QVERIFY(bluetoothIndicator != nullptr);
    QObject *bluetoothIcon = bluetoothIndicator->findChild<QObject *>(QStringLiteral("bluetoothIcon"));
    QObject *scanPulse = bluetoothIndicator->findChild<QObject *>(QStringLiteral("scanPulse"));
    QVERIFY(bluetoothIcon != nullptr);
    QVERIFY(scanPulse != nullptr);
    bluetooth.setState(false, false, false, 0, false);
    QCoreApplication::processEvents();
    QCOMPARE(bluetoothIcon->property("text").toString(), QStringLiteral("󰂲"));
    bluetooth.setState(true, true, true, 1, true);
    QCoreApplication::processEvents();
    QCOMPARE(bluetoothIcon->property("text").toString(), QStringLiteral("󰂯"));
    QVERIFY(scanPulse->property("visible").toBool());

    QQmlComponent volumeComponent(
        &engine, QUrl(QStringLiteral(
            "qrc:/qt/qml/Astrea/Shell/Bar/qml/components/VolumeIndicator.qml")));
    QObject *volumeIndicator = volumeComponent.createWithInitialProperties({
        {QStringLiteral("audioService"), QVariant::fromValue(&audio)},
    });
    QVERIFY(volumeIndicator != nullptr);
    QObject *volumeIcon = volumeIndicator->findChild<QObject *>(QStringLiteral("volumeIcon"));
    QVERIFY(volumeIcon != nullptr);
    audio.setVolumeForTest(0);
    QCoreApplication::processEvents();
    QCOMPARE(volumeIcon->property("text").toString(), QStringLiteral("󰝟"));
    audio.setVolumeForTest(50);
    QCoreApplication::processEvents();
    QCOMPARE(volumeIcon->property("text").toString(), QStringLiteral("󰖀"));
    audio.setVolumeForTest(90);
    QCoreApplication::processEvents();
    QCOMPARE(volumeIcon->property("text").toString(), QStringLiteral("󰕾"));
    audio.setMuted(true);
    QCoreApplication::processEvents();
    QCOMPARE(volumeIcon->property("text").toString(), QStringLiteral("󰝟"));

    delete volumeIndicator;
    delete bluetoothIndicator;
    delete networkIndicator;
}

void BarQmlSmokeTest::workspaceDelegatesExposeReferenceHitboxes()
{
    QQmlEngine engine;
    WorkspaceModel model;
    model.replaceWorkspaces({
        {QStringLiteral("1"), true, true, false, {}},
        {QStringLiteral("2"), false, false, false, {}},
        {QStringLiteral("3"), false, true, true, {}},
    });
    QQmlComponent component(
        &engine, QUrl(QStringLiteral(
            "qrc:/qt/qml/Astrea/Shell/Bar/qml/components/WorkspaceStrip.qml")));
    QObject *object = component.createWithInitialProperties({
        {QStringLiteral("workspaceModel"), QVariant::fromValue(&model)},
    });
    QVERIFY(object != nullptr);
    QCoreApplication::processEvents();
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(object->property("workspaceModel").value<QObject *>(),
             static_cast<QObject *>(&model));
    QObject *repeater = object->findChild<QObject *>(QStringLiteral("workspaceRepeater"));
    QVERIFY(repeater != nullptr);
    const int delegateCount = repeater->property("count").toInt();
    QCOMPARE(delegateCount, 3);
    QList<QObject *> hitboxes;
    for (int i = 0; i < delegateCount; ++i) {
        QQuickItem *delegate = nullptr;
        QVERIFY(QMetaObject::invokeMethod(repeater, "itemAt", Qt::DirectConnection,
                                          Q_RETURN_ARG(QQuickItem *, delegate),
                                          Q_ARG(int, i)));
        QVERIFY(delegate != nullptr);
        hitboxes.append(delegate->findChildren<QObject *>(
            QStringLiteral("workspaceHitTarget")));
    }
    QCOMPARE(hitboxes.size(), 3);
    for (QObject *hitbox : hitboxes) {
        QVERIFY(hitbox->property("hoverEnabled").toBool());
        QCOMPARE(hitbox->property("cursorShape").toInt(),
                 static_cast<int>(Qt::PointingHandCursor));
    }
    delete object;
}

void BarQmlSmokeTest::popupVisualsUseNativeServiceInputs()
{
    QQmlEngine engine;
    BarLayoutMetrics metrics;
    BarPopupController popup;
    FakeAudioService audio;
    FakeNetworkService network;
    FakeBluetoothService bluetooth;
    QQmlComponent component(
        &engine, QUrl(QStringLiteral(
            "qrc:/qt/qml/Astrea/Shell/Bar/qml/PopupOverlaySurface.qml")));
    QObject *overlay = component.createWithInitialProperties({
        {QStringLiteral("barGeometry"), QVariant::fromValue(&metrics)},
        {QStringLiteral("popupController"), QVariant::fromValue(&popup)},
        {QStringLiteral("audioService"), QVariant::fromValue(&audio)},
        {QStringLiteral("networkService"), QVariant::fromValue(&network)},
        {QStringLiteral("bluetoothService"), QVariant::fromValue(&bluetooth)},
        {QStringLiteral("outputWidth"), 800},
        {QStringLiteral("outputHeight"), 600},
    });
    QVERIFY2(overlay != nullptr, qPrintable(component.errors().isEmpty()
        ? QStringLiteral("Popup overlay did not instantiate")
        : component.errors().constFirst().toString()));

    QObject *menu = overlay->findChild<QObject *>(QStringLiteral("astreaMenu"));
    QVERIFY(menu != nullptr);
    QVERIFY(menu->findChild<QObject *>(QStringLiteral("menuItem")) != nullptr);
    QVERIFY(menu->findChild<QObject *>(QStringLiteral("menuSeparator")) != nullptr);

    popup.open(BarPopupController::PopupKind::Network, 400);
    QCoreApplication::processEvents();
    QObject *networkPopup = overlay->findChild<QObject *>(QStringLiteral("networkPopup"));
    QVERIFY(networkPopup != nullptr);
    QVERIFY(networkPopup->property("visible").toBool());
    QCOMPARE(networkPopup->property("width").toInt(), metrics.popupWidth(800, 280));

    popup.open(BarPopupController::PopupKind::Bluetooth, 400);
    QCoreApplication::processEvents();
    QObject *bluetoothPopup = overlay->findChild<QObject *>(QStringLiteral("bluetoothPopup"));
    QVERIFY(bluetoothPopup != nullptr);
    QVERIFY(bluetoothPopup->property("visible").toBool());

    popup.open(BarPopupController::PopupKind::Volume, 400);
    QCoreApplication::processEvents();
    QObject *volumePopup = overlay->findChild<QObject *>(QStringLiteral("volumePopup"));
    QVERIFY(volumePopup != nullptr);
    QVERIFY(volumePopup->property("visible").toBool());
    QVERIFY(volumePopup->findChild<QObject *>(QStringLiteral("volumeSlider")) != nullptr);

    delete overlay;
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication application(argc, argv);
    BarQmlSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "BarQmlSmokeTest.moc"
