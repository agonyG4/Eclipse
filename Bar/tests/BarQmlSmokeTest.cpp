#include "core/BarClockService.hpp"
#include "core/BarLayoutMetrics.hpp"
#include "core/BarPopupController.hpp"
#include "core/WorkspaceModel.hpp"
#include "theme/ThemeController.hpp"

#include <QAbstractListModel>
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
    Q_PROPERTY(bool defaultStateAvailable READ defaultStateAvailable NOTIFY defaultStateAvailableChanged)

public:
    explicit FakeAudioService(QObject *parent = nullptr) : QObject(parent) {}
    double volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    bool defaultStateAvailable() const { return m_defaultStateAvailable; }
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
    void setDefaultStateAvailableForTest(bool available) {
        if (m_defaultStateAvailable == available)
            return;
        m_defaultStateAvailable = available;
        emit defaultStateAvailableChanged();
    }
    double m_lastDelta = 0.0;

signals:
    void volumeChanged();
    void mutedChanged();
    void defaultStateAvailableChanged();

private:
    double m_volume = 50.0;
    bool m_muted = false;
    bool m_defaultStateAvailable = true;
};

class FakeNetworkService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool wifiAvailable READ wifiAvailable NOTIFY changed)
    Q_PROPERTY(int connectionType READ connectionType NOTIFY changed)
    Q_PROPERTY(QString connectionName READ connectionName NOTIFY changed)

public:
    explicit FakeNetworkService(QObject *parent = nullptr) : QObject(parent) {}
    bool available() const { return m_available; }
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

    void setAvailable(bool available)
    {
        m_available = available;
        emit changed();
    }

signals:
    void changed();

private:
    bool m_available = true;
    bool m_connected = true;
    bool m_wifiAvailable = true;
    int m_connectionType = 1;
    QString m_connectionName = QStringLiteral("Office");
};

class FakeBluetoothDeviceModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        ObjectPathRole,
        NameRole,
        PairedRole,
        ConnectedRole,
        RssiRole,
        BatteryPercentRole,
    };

    struct Device {
        QString id;
        QString objectPath;
        QString name;
        bool paired = false;
        bool connected = false;
        int rssi = -1;
        int batteryPercent = -1;
    };

    explicit FakeBluetoothDeviceModel(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
    }

    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : m_devices.size();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_devices.size())
            return {};
        const Device &device = m_devices.at(index.row());
        switch (role) {
        case IdRole: return device.id;
        case ObjectPathRole: return device.objectPath;
        case NameRole: return device.name;
        case PairedRole: return device.paired;
        case ConnectedRole: return device.connected;
        case RssiRole: return device.rssi;
        case BatteryPercentRole: return device.batteryPercent;
        case Qt::DisplayRole: return device.name;
        default: return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {
            {IdRole, "id"},
            {ObjectPathRole, "objectPath"},
            {NameRole, "name"},
            {PairedRole, "paired"},
            {ConnectedRole, "connected"},
            {RssiRole, "rssi"},
            {BatteryPercentRole, "batteryPercent"},
        };
    }

    void replace(QVector<Device> devices)
    {
        beginResetModel();
        m_devices = std::move(devices);
        endResetModel();
    }

private:
    QVector<Device> m_devices;
};

class FakeBluetoothService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool adapterAvailable READ adapterAvailable NOTIFY changed)
    Q_PROPERTY(bool powered READ powered NOTIFY changed)
    Q_PROPERTY(int connectedCount READ connectedCount NOTIFY changed)
    Q_PROPERTY(bool scanning READ scanning NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel *devicesModel READ devicesModel CONSTANT)

public:
    explicit FakeBluetoothService(QObject *parent = nullptr)
        : QObject(parent)
        , m_devicesModel(new FakeBluetoothDeviceModel(this))
    {
    }
    bool available() const { return m_available; }
    bool powered() const { return m_powered; }
    bool adapterAvailable() const { return m_adapterAvailable; }
    int connectedCount() const { return m_connectedCount; }
    bool scanning() const { return m_scanning; }
    QAbstractItemModel *devicesModel() const { return m_devicesModel; }

    void setDevices(QVector<FakeBluetoothDeviceModel::Device> devices)
    {
        m_devicesModel->replace(std::move(devices));
        emit changed();
    }

    Q_INVOKABLE bool setPowered(bool powered)
    {
        m_powered = powered;
        emit changed();
        return true;
    }

    Q_INVOKABLE bool requestScan(const QString &)
    {
        m_scanning = true;
        emit changed();
        return true;
    }

    Q_INVOKABLE void releaseScan(const QString &)
    {
        m_scanning = false;
        emit changed();
    }

    Q_INVOKABLE bool connectDevice(const QString &objectPath)
    {
        m_lastDeviceAction = QStringLiteral("connect:") + objectPath;
        return true;
    }

    Q_INVOKABLE bool disconnectDevice(const QString &objectPath)
    {
        m_lastDeviceAction = QStringLiteral("disconnect:") + objectPath;
        return true;
    }

    QString lastDeviceAction() const { return m_lastDeviceAction; }

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
    bool m_scanning = false;
    FakeBluetoothDeviceModel *m_devicesModel = nullptr;
    QString m_lastDeviceAction;
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
    void bluetoothIndicatorCentersGlyphInVisualContainer();
    void volumeSliderAnimatesProgrammaticChangesOnly();
    void networkPopupProjectsOneCoherentConnectionState();
    void menuItemMatchesReferenceRightPadding();
    void bluetoothPopupOrganizesPairedAndAvailableDevices();
    void popupOverlayRejectsUnsupportedKinds();
    void workspaceDelegatesExposeReferenceHitboxes();
    void workspaceAndLauncherReserveStableWidth();
    void workspaceActivationIsTruthfullyUnavailable();
    void popupVisualsUseNativeServiceInputs();
    void volumeUiDisablesWhenDefaultStateUnavailable();

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
    QTRY_COMPARE_WITH_TIMEOUT(color(), QColor::fromRgbF(0, 0, 0, 0.06), 500);
    QTRY_COMPARE_WITH_TIMEOUT(borderColor(), QColor::fromRgbF(1, 1, 1, 0.28), 500);

    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, QPoint(30, 25));
    QCoreApplication::processEvents();
    QTRY_COMPARE_WITH_TIMEOUT(color(), QColor::fromRgbF(0, 0, 0, 0.06), 500);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, QPoint(30, 25));

    segment->setProperty("active", true);
    QCoreApplication::processEvents();
    QTRY_COMPARE_WITH_TIMEOUT(color(), QColor::fromRgbF(0, 0, 0, 0.06), 500);
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
    QCOMPARE(overlay->property("hiddenScale").toReal(), 0.97);
    QCOMPARE(overlay->property("fadeDuration").toInt(), 180);
    QCOMPARE(overlay->property("scaleDuration").toInt(), 220);
    QCOMPARE(menu->property("cardPadding").toInt(), 12);
    QCOMPARE(menu->property("contentSpacing").toInt(), 4);
    QCOMPARE(menu->property("radius").toReal(), 14.0);
    QCOMPARE(menu->property("backgroundColor").value<QColor>(),
             QColor::fromRgbF(0, 0, 0, 0.06));
    QCOMPARE(menu->property("borderColor").value<QColor>(),
             QColor::fromRgbF(1, 1, 1, 0.14));
    QObject *enterFade = overlay->findChild<QObject *>(QStringLiteral("popupEnterFade"));
    QObject *enterScale = overlay->findChild<QObject *>(QStringLiteral("popupEnterScale"));
    QObject *exitFade = overlay->findChild<QObject *>(QStringLiteral("popupExitFade"));
    QObject *exitScale = overlay->findChild<QObject *>(QStringLiteral("popupExitScale"));
    QVERIFY(enterFade != nullptr);
    QVERIFY(enterScale != nullptr);
    QVERIFY(exitFade != nullptr);
    QVERIFY(exitScale != nullptr);
    QCOMPARE(enterFade->property("duration").toInt(), 180);
    QCOMPARE(enterScale->property("duration").toInt(), 220);
    QCOMPARE(exitFade->property("duration").toInt(), 180);
    QCOMPARE(exitScale->property("duration").toInt(), 220);

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
    QVERIFY(overlay->findChild<QObject *>(QStringLiteral("clockPopup")) == nullptr);
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
    popup.open(BarPopupController::PopupKind::AstreaMenu, 500);
    QTest::qWait(40);
    popup.close();
    QCoreApplication::processEvents();
    QVERIFY(popup.closing());
    popup.open(BarPopupController::PopupKind::AstreaMenu, 520);
    QCoreApplication::processEvents();
    QVERIFY(popup.isOpen());
    QVERIFY(!popup.closing());
    QTest::qWait(300);
    QVERIFY(popup.surfaceRequired());
    QCOMPARE(popup.kind(), BarPopupController::PopupKind::AstreaMenu);
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
    popup.open(BarPopupController::PopupKind::AstreaMenu, 500);
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
    QObject *clockButton = status->findChild<QObject *>(QStringLiteral("clockButton"));
    QObject *date = status->findChild<QObject *>(QStringLiteral("clockDate"));
    QObject *separator = status->findChild<QObject *>(QStringLiteral("clockSeparator"));
    QObject *time = status->findChild<QObject *>(QStringLiteral("clockTime"));
    QObject *row = status->findChild<QObject *>(QStringLiteral("clockRow"));
    QVERIFY(clockObject != nullptr);
    QVERIFY(clockButton != nullptr);
    QVERIFY(date != nullptr);
    QVERIFY(separator != nullptr);
    QVERIFY(time != nullptr);
    QVERIFY(row != nullptr);
    QCOMPARE(separator->property("width").toInt(), 1);
    QVERIFY(separator->property("height").toInt() > separator->property("width").toInt());
    QCOMPARE(date->property("y").toInt(), time->property("y").toInt());
    QCOMPARE(row->property("spacing").toInt(), 0);
    QCOMPARE(clockObject->property("height").toInt(), 36);
    QVERIFY(!clockButton->property("interactive").toBool());
    QCOMPARE(date->property("width").toReal() - date->property("implicitWidth").toReal(), 8.0);
    QCOMPARE(time->property("width").toReal() - time->property("implicitWidth").toReal(), 16.0);
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
    QCOMPARE(logoButton->property("width").toInt(), 28);
    QCOMPARE(logoButton->property("height").toInt(), 28);
    QObject *logoImage = launcher->findChild<QObject *>(QStringLiteral("logoImage"));
    QVERIFY(logoImage != nullptr);
    QCOMPARE(logoImage->property("width").toInt(), 18);
    QCOMPARE(logoImage->property("height").toInt(), 18);
    QCOMPARE(logoImage->property("opacity").toReal(), 0.80);

    auto *launcherWindow = qobject_cast<QQuickWindow *>(launcher);
    QVERIFY(launcherWindow != nullptr);
    launcherWindow->show();
    QTest::qWait(20);
    QTest::mouseClick(launcherWindow, Qt::LeftButton, Qt::NoModifier,
                      QPoint(qMax(0, launcherWindow->width() - 4), launcherWindow->height() / 2));
    QCoreApplication::processEvents();
    QVERIFY(!popup.isOpen());
    const QPointF logoPoint = qobject_cast<QQuickItem *>(logoButton)->mapToItem(
        launcherWindow->contentItem(), QPointF(logoButton->property("width").toReal() / 2,
                                               logoButton->property("height").toReal() / 2));
    QTest::mouseClick(launcherWindow, Qt::LeftButton, Qt::NoModifier, logoPoint.toPoint());
    QTRY_VERIFY_WITH_TIMEOUT(popup.isOpen(), 500);
    QCOMPARE(popup.kind(), BarPopupController::PopupKind::AstreaMenu);
    popup.clearForOutput();

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
        if (name == QStringLiteral("clock"))
            continue;
        QVERIFY2(indicator->property("interactive").toBool(), qPrintable(name));
    }

    QObject *networkIndicator = status->findChild<QObject *>(QStringLiteral("networkIndicator"));
    QVERIFY(networkIndicator != nullptr);
    auto *statusWindow = qobject_cast<QQuickWindow *>(status);
    QVERIFY(statusWindow != nullptr);
    statusWindow->show();
    QTest::qWait(20);
    auto *clockItem = qobject_cast<QQuickItem *>(status->findChild<QObject *>(QStringLiteral("clock")));
    QVERIFY(clockItem != nullptr);
    const QPointF clockPoint = clockItem->mapToItem(
        statusWindow->contentItem(), QPointF(clockItem->width() / 2, clockItem->height() / 2));
    QTest::mouseClick(statusWindow, Qt::LeftButton, Qt::NoModifier, clockPoint.toPoint());
    QCoreApplication::processEvents();
    QVERIFY(!popup.isOpen());
    const auto clickIndicator = [statusWindow](QObject *object) {
        auto *item = qobject_cast<QQuickItem *>(object);
        QVERIFY(item != nullptr);
        const QPointF point = item->mapToItem(
            statusWindow->contentItem(), QPointF(item->width() / 2, item->height() / 2));
        QTest::mouseClick(statusWindow, Qt::LeftButton, Qt::NoModifier, point.toPoint());
        QCoreApplication::processEvents();
    };
    clickIndicator(networkIndicator);
    QTRY_COMPARE_WITH_TIMEOUT(popup.kind(), BarPopupController::PopupKind::Network, 500);
    QVERIFY(networkIndicator->property("active").toBool());
    popup.close();
    QCoreApplication::processEvents();
    QVERIFY(networkIndicator->property("active").toBool());
    popup.completeClose();
    clickIndicator(status->findChild<QObject *>(QStringLiteral("bluetoothIndicator")));
    QTRY_COMPARE_WITH_TIMEOUT(popup.kind(), BarPopupController::PopupKind::Bluetooth, 500);
    popup.completeClose();
    clickIndicator(status->findChild<QObject *>(QStringLiteral("volumeIndicator")));
    QTRY_COMPARE_WITH_TIMEOUT(popup.kind(), BarPopupController::PopupKind::Volume, 500);
    popup.clearForOutput();

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
    QCOMPARE(scanPulse->property("width").toReal(), 16.0);
    QCOMPARE(scanPulse->property("height").toReal(), 16.0);
    QCOMPARE(QQmlProperty(scanPulse, QStringLiteral("border.width")).read().toReal(), 1.5);
    QCOMPARE(bluetoothIndicator->property("scanPulseAnimationDuration").toInt(), 900);
    QCOMPARE(bluetoothIndicator->property("scanPulseScaleEasing").toInt(), 6);
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

void BarQmlSmokeTest::bluetoothIndicatorCentersGlyphInVisualContainer()
{
    QQmlEngine engine;
    FakeBluetoothService bluetooth;
    QQmlComponent component(
        &engine, QUrl(QStringLiteral(
            "qrc:/qt/qml/Astrea/Shell/Bar/qml/components/BluetoothIndicator.qml")));
    auto *indicator = qobject_cast<QQuickItem *>(component.createWithInitialProperties({
        {QStringLiteral("bluetoothService"), QVariant::fromValue(&bluetooth)},
    }));
    QVERIFY(indicator != nullptr);

    QQuickWindow window;
    window.resize(120, 50);
    indicator->setParentItem(window.contentItem());
    indicator->setWidth(28);
    indicator->setHeight(34);
    window.show();
    QTest::qWait(20);

    auto *visual = qobject_cast<QQuickItem *>(indicator->findChild<QObject *>(
        QStringLiteral("bluetoothVisual")));
    auto *icon = qobject_cast<QQuickItem *>(indicator->findChild<QObject *>(
        QStringLiteral("bluetoothIcon")));
    QVERIFY(visual != nullptr);
    QVERIFY(icon != nullptr);
    QCOMPARE(visual->width(), 16.0);
    QCOMPARE(visual->height(), 16.0);
    const QPointF visualCenter = visual->mapToItem(
        indicator, QPointF(visual->width() / 2.0, visual->height() / 2.0));
    const QPointF iconCenter = icon->mapToItem(
        indicator, QPointF(icon->width() / 2.0, icon->height() / 2.0));
    const qreal visualCenterX = visualCenter.x();
    const qreal visualCenterY = visualCenter.y();
    const qreal iconCenterX = iconCenter.x();
    const qreal iconCenterY = iconCenter.y();
    QVERIFY(QQmlProperty(icon, QStringLiteral("anchors.centerIn")).isValid());
    QVERIFY(qAbs(iconCenterX - visualCenterX) < 0.25);
    QVERIFY(qAbs(iconCenterY - visualCenterY) < 0.01);

    delete indicator;
}

void BarQmlSmokeTest::volumeSliderAnimatesProgrammaticChangesOnly()
{
    QQmlEngine engine;
    FakeAudioService audio;
    QQmlComponent component(
        &engine, QUrl(QStringLiteral(
            "qrc:/qt/qml/Astrea/Shell/Bar/qml/VolumePopup.qml")));
    auto *popup = qobject_cast<QQuickItem *>(component.createWithInitialProperties({
        {QStringLiteral("audioService"), QVariant::fromValue(&audio)},
    }));
    QVERIFY(popup != nullptr);
    auto *slider = qobject_cast<QQuickItem *>(popup->findChild<QObject *>(
        QStringLiteral("volumeSlider")));
    auto *sliderMouse = qobject_cast<QQuickItem *>(popup->findChild<QObject *>(
        QStringLiteral("volumeSliderMouse")));
    auto *thumb = qobject_cast<QQuickItem *>(popup->findChild<QObject *>(
        QStringLiteral("volumeThumb")));
    QObject *thumbBehavior = popup->findChild<QObject *>(
        QStringLiteral("volumeThumbXBehavior"));
    QVERIFY(slider != nullptr);
    QVERIFY(sliderMouse != nullptr);
    QVERIFY(thumb != nullptr);
    QVERIFY(thumbBehavior != nullptr);

    QQuickWindow window;
    window.resize(340, 120);
    popup->setParentItem(window.contentItem());
    popup->setWidth(300);
    popup->setHeight(popup->implicitHeight());
    window.show();
    QTest::qWait(20);

    const qreal oldX = thumb->x();
    audio.setVolumeForTest(75.0);
    QCoreApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(thumb->x() != oldX, 500);
    QVERIFY(thumbBehavior->property("enabled").toBool());

    const QPointF sliderPoint = sliderMouse->mapToItem(
        window.contentItem(), QPointF(sliderMouse->width() / 2.0,
                                      sliderMouse->height() / 2.0));
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, sliderPoint.toPoint());
    QCoreApplication::processEvents();
    QVERIFY(!thumbBehavior->property("enabled").toBool());
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, sliderPoint.toPoint());

    delete popup;
}

void BarQmlSmokeTest::networkPopupProjectsOneCoherentConnectionState()
{
    QQmlEngine engine;
    FakeNetworkService network;
    QQmlComponent component(
        &engine, QUrl(QStringLiteral(
            "qrc:/qt/qml/Astrea/Shell/Bar/qml/NetworkPopup.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errors().isEmpty()
                 ? QStringLiteral("NetworkPopup is not ready")
                 : component.errors().constFirst().toString()));
    QObject *popup = component.createWithInitialProperties({
        {QStringLiteral("networkService"), QVariant::fromValue(&network)},
    });
    QVERIFY(popup != nullptr);

    QObject *header = popup->findChild<QObject *>(QStringLiteral("networkPopupHeader"));
    QVERIFY(header != nullptr);
    auto state = [popup] { return popup->property("connectionState").toInt(); };
    auto title = [popup] { return popup->property("connectionTitle").toString(); };
    auto icon = [header] { return header->property("icon").toString(); };

    network.setAvailable(false);
    QCoreApplication::processEvents();
    QCOMPARE(state(), 0);
    QCOMPARE(title(), QStringLiteral("Network"));
    QCOMPARE(icon(), QStringLiteral("󰖪"));

    network.setAvailable(true);
    network.setState(false, true, 0, {});
    QCoreApplication::processEvents();
    QCOMPARE(state(), 1);
    QCOMPARE(title(), QStringLiteral("Network"));
    QCOMPARE(icon(), QStringLiteral("󰖪"));

    network.setState(true, true, 1, QStringLiteral("Office Wi-Fi"));
    QCoreApplication::processEvents();
    QCOMPARE(state(), 2);
    QCOMPARE(title(), QStringLiteral("Office Wi-Fi"));
    QCOMPARE(icon(), QStringLiteral("󰖩"));

    network.setState(true, false, 2, QStringLiteral("Ethernet"));
    QCoreApplication::processEvents();
    QCOMPARE(state(), 3);
    QCOMPARE(icon(), QStringLiteral("󰈀"));

    network.setState(true, false, 3, QStringLiteral("VPN"));
    QCoreApplication::processEvents();
    QCOMPARE(state(), 4);
    QCOMPARE(title(), QStringLiteral("VPN"));
    QVERIFY(icon() != QStringLiteral("󰈀"));

    delete popup;
}

void BarQmlSmokeTest::menuItemMatchesReferenceRightPadding()
{
    QQmlEngine engine;
    QQmlComponent component(
        &engine, QUrl(QStringLiteral(
            "qrc:/qt/qml/Astrea/Shell/Bar/qml/components/MenuItem.qml")));
    QObject *item = component.createWithInitialProperties({
        {QStringLiteral("icon"), QStringLiteral("󰍉")},
        {QStringLiteral("text"), QStringLiteral("A long menu item that must elide")},
    });
    QVERIFY(item != nullptr);
    item->setProperty("width", 240);
    item->setProperty("enabled", false);
    QCoreApplication::processEvents();

    QObject *row = item->findChild<QObject *>(QStringLiteral("menuItemRow"));
    QObject *iconSlot = item->findChild<QObject *>(QStringLiteral("menuItemIconSlot"));
    QObject *text = item->findChild<QObject *>(QStringLiteral("menuItemText"));
    QVERIFY(row != nullptr);
    QVERIFY(iconSlot != nullptr);
    QVERIFY(text != nullptr);
    QCOMPARE(row->property("x").toReal(), 12.0);
    QVERIFY(iconSlot->property("width").toReal() > 0.0);
    QCOMPARE(text->property("width").toReal(),
             row->property("width").toReal() - text->property("x").toReal() - 12.0);
    QCOMPARE(text->property("elide").toInt(), static_cast<int>(Qt::ElideRight));

    item->setProperty("icon", QString());
    item->setProperty("text", QStringLiteral("Short"));
    QCoreApplication::processEvents();
    QCOMPARE(iconSlot->property("width").toReal(), 0.0);
    QCOMPARE(text->property("width").toReal(),
             row->property("width").toReal() - text->property("x").toReal() - 12.0);
    QVERIFY(!item->property("enabled").toBool());
    delete item;
}

void BarQmlSmokeTest::bluetoothPopupOrganizesPairedAndAvailableDevices()
{
    QQmlEngine engine;
    FakeBluetoothService bluetooth;
    QQmlComponent component(
        &engine, QUrl(QStringLiteral(
            "qrc:/qt/qml/Astrea/Shell/Bar/qml/BluetoothPopup.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errors().isEmpty()
                 ? QStringLiteral("BluetoothPopup is not ready")
                 : component.errors().constFirst().toString()));
    QObject *popup = component.createWithInitialProperties({
        {QStringLiteral("bluetoothService"), QVariant::fromValue(&bluetooth)},
    });
    QVERIFY(popup != nullptr);
    QObject *pairedSection = popup->findChild<QObject *>(QStringLiteral("pairedSection"));
    QObject *availableSection = popup->findChild<QObject *>(QStringLiteral("availableSection"));
    QObject *separator = popup->findChild<QObject *>(QStringLiteral("deviceScanSeparator"));
    QObject *scanAction = popup->findChild<QObject *>(QStringLiteral("scanAction"));
    QVERIFY(pairedSection != nullptr);
    QVERIFY(availableSection != nullptr);
    QVERIFY(separator != nullptr);
    QVERIFY(scanAction != nullptr);

    using Device = FakeBluetoothDeviceModel::Device;
    bluetooth.setDevices({});
    QCoreApplication::processEvents();
    QVERIFY(!pairedSection->property("visible").toBool());
    QVERIFY(!availableSection->property("visible").toBool());

    bluetooth.setDevices({Device{QStringLiteral("headphones"), QStringLiteral("/paired"),
                                 QStringLiteral("Headphones"), true, false, -42, 80}});
    QTRY_VERIFY_WITH_TIMEOUT(pairedSection->property("visible").toBool(), 500);
    QVERIFY(!availableSection->property("visible").toBool());
    QVERIFY(separator->property("visible").toBool());

    bluetooth.setDevices({Device{QStringLiteral("keyboard"), QStringLiteral("/available"),
                                 QStringLiteral("Keyboard"), false, false, -60, -1}});
    QTRY_VERIFY_WITH_TIMEOUT(availableSection->property("visible").toBool(), 500);
    QVERIFY(!pairedSection->property("visible").toBool());
    auto *availableRows = qobject_cast<QQuickItem *>(popup->findChild<QObject *>(
        QStringLiteral("availableRows")));
    QVERIFY(availableRows != nullptr);
    QQuickItem *availableRow = nullptr;
    for (QQuickItem *child : availableRows->childItems()) {
        if (child->objectName() == QStringLiteral("availableDeviceRow")) {
            availableRow = child;
            break;
        }
    }
    QVERIFY(availableRow != nullptr);
    QObject *availableMouse = availableRow->findChild<QObject *>(QStringLiteral("deviceMouse"));
    QVERIFY(availableMouse != nullptr);
    QVERIFY(!availableMouse->property("enabled").toBool());
    QVERIFY(popup->findChild<QObject *>(QStringLiteral("pairButton")) == nullptr);

    bluetooth.setDevices({
        Device{QStringLiteral("headphones"), QStringLiteral("/paired"),
               QStringLiteral("Headphones"), true, true, -42, 80},
        Device{QStringLiteral("keyboard"), QStringLiteral("/available"),
               QStringLiteral("Keyboard"), false, false, -60, -1},
    });
    QTRY_VERIFY_WITH_TIMEOUT(pairedSection->property("visible").toBool(), 500);
    QTRY_VERIFY_WITH_TIMEOUT(availableSection->property("visible").toBool(), 500);

    bluetooth.setState(true, true, true, 1, false);
    QCoreApplication::processEvents();
    QCOMPARE(QQmlProperty(scanAction, QStringLiteral("border.width")).read().toInt(), 0);
    bluetooth.setState(true, true, true, 1, true);
    QTRY_VERIFY_WITH_TIMEOUT(popup->property("scanning").toBool(), 500);
    QTRY_COMPARE_WITH_TIMEOUT(scanAction->property("color").value<QColor>(),
                              QColor::fromRgbF(0.20, 0.60, 1.0, 0.10), 500);
    QTRY_COMPARE_WITH_TIMEOUT(
        QQmlProperty(scanAction, QStringLiteral("border.width")).read().toInt(), 1, 500);
    QVERIFY(separator->property("visible").toBool());

    delete popup;
}

void BarQmlSmokeTest::popupOverlayRejectsUnsupportedKinds()
{
    QQmlEngine engine;
    BarLayoutMetrics metrics;
    BarPopupController popup;
    QQmlComponent component(&engine, QUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Shell/Bar/qml/PopupOverlaySurface.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errors().isEmpty()
                 ? QStringLiteral("PopupOverlaySurface is not ready")
                 : component.errors().constFirst().toString()));
    QObject *overlay = component.createWithInitialProperties({
        {QStringLiteral("barGeometry"), QVariant::fromValue(&metrics)},
        {QStringLiteral("popupController"), QVariant::fromValue(&popup)},
    });
    QVERIFY(overlay != nullptr);
    QObject *shield = overlay->findChild<QObject *>(QStringLiteral("popupClickShield"));
    QVERIFY(shield != nullptr);

    const auto unsupported = static_cast<BarPopupController::PopupKind>(2);
    popup.open(unsupported, 400);
    QCoreApplication::processEvents();
    QVERIFY(!popup.isOpen());
    QVERIFY(!popup.surfaceRequired());
    QVERIFY(overlay->findChild<QObject *>(QStringLiteral("astreaMenu"))
                ->property("visible").toBool() == false);
    QVERIFY(!shield->property("enabled").toBool());

    popup.open(BarPopupController::PopupKind::AstreaMenu, 400);
    QCoreApplication::processEvents();
    QVERIFY(popup.isOpen());
    QVERIFY(overlay->findChild<QObject *>(QStringLiteral("astreaMenu"))
                ->property("visible").toBool());
    QVERIFY(shield->property("enabled").toBool());
    delete overlay;
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
        {QStringLiteral("activationAvailable"), false},
    });
    QVERIFY(object != nullptr);
    QCoreApplication::processEvents();
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(object->property("workspaceModel").value<QObject *>(),
             static_cast<QObject *>(&model));
    QVERIFY(object->property("clip").toBool());
    QObject *repeater = object->findChild<QObject *>(QStringLiteral("workspaceRepeater"));
    QVERIFY(repeater != nullptr);
    const int delegateCount = repeater->property("count").toInt();
    QCOMPARE(delegateCount, 3);
    QObject *row = object->findChild<QObject *>(QStringLiteral("workspaceRow"));
    QVERIFY(row != nullptr);
    QCOMPARE(row->property("spacing").toReal(), 6.0);
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
        QVERIFY(!hitbox->property("hoverEnabled").toBool());
        QCOMPARE(hitbox->property("cursorShape").toInt(),
                 static_cast<int>(Qt::ArrowCursor));
    }
    QQuickItem *activeDelegate = nullptr;
    QQuickItem *inactiveDelegate = nullptr;
    QVERIFY(QMetaObject::invokeMethod(repeater, "itemAt", Qt::DirectConnection,
                                      Q_RETURN_ARG(QQuickItem *, activeDelegate), Q_ARG(int, 0)));
    QVERIFY(QMetaObject::invokeMethod(repeater, "itemAt", Qt::DirectConnection,
                                      Q_RETURN_ARG(QQuickItem *, inactiveDelegate), Q_ARG(int, 1)));
    QCOMPARE(activeDelegate->property("width").toInt(), 32);
    QCOMPARE(inactiveDelegate->property("width").toInt(), 10);
    QCOMPARE(activeDelegate->findChild<QObject *>(QStringLiteral("workspaceDot"))
                 ->property("color").value<QColor>(), QColor(QStringLiteral("#ffffffff")));
    QCOMPARE(inactiveDelegate->findChild<QObject *>(QStringLiteral("workspaceDot"))
                 ->property("color").value<QColor>(), QColor::fromRgbF(1, 1, 1, 0.22));
    QCOMPARE(activeDelegate->property("workspaceWidthAnimationDuration").toInt(), 200);
    QCOMPARE(activeDelegate->property("workspaceColorAnimationDuration").toInt(), 180);
    QCOMPARE(activeDelegate->property("workspaceWidthAnimationEasing").toInt(), 22);
    delete object;
}

void BarQmlSmokeTest::workspaceAndLauncherReserveStableWidth()
{
    QQmlEngine engine;
    BarLayoutMetrics metrics;
    BarPopupController popup;
    WorkspaceModel model;
    model.replaceWorkspaces({
        {QStringLiteral("1"), true, true, false, {}},
        {QStringLiteral("2"), false, false, false, {}},
        {QStringLiteral("3"), false, false, false, {}},
        {QStringLiteral("4"), false, false, false, {}},
    });

    QQmlComponent stripComponent(&engine, QUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Shell/Bar/qml/components/WorkspaceStrip.qml")));
    QObject *strip = stripComponent.createWithInitialProperties({
        {QStringLiteral("workspaceModel"), QVariant::fromValue(&model)},
    });
    QVERIFY(strip != nullptr);
    QCoreApplication::processEvents();
    QCOMPARE(strip->property("stableWidth").toInt(), 80);
    QCOMPARE(strip->property("width").toInt(), 80);
    const int firstWidth = strip->property("width").toInt();

    model.replaceWorkspaces({
        {QStringLiteral("1"), false, true, false, {}},
        {QStringLiteral("2"), false, false, false, {}},
        {QStringLiteral("3"), false, false, false, {}},
        {QStringLiteral("4"), true, false, false, {}},
    });
    QTRY_COMPARE_WITH_TIMEOUT(strip->property("width").toInt(), firstWidth, 500);
    model.replaceWorkspaces({
        {QStringLiteral("1"), false, true, false, {}},
        {QStringLiteral("2"), false, false, false, {}},
        {QStringLiteral("3"), false, false, false, {}},
        {QStringLiteral("4"), false, false, false, {}},
    });
    QTRY_COMPARE_WITH_TIMEOUT(strip->property("width").toInt(), firstWidth, 500);
    model.replaceWorkspaces({});
    QTRY_COMPARE_WITH_TIMEOUT(strip->property("width").toInt(), 0, 500);
    QVERIFY(strip->property("width").toInt() >= 0);
    delete strip;

    model.replaceWorkspaces({
        {QStringLiteral("1"), true, true, false, {}},
        {QStringLiteral("2"), false, false, false, {}},
        {QStringLiteral("3"), false, false, false, {}},
        {QStringLiteral("4"), false, false, false, {}},
    });
    QQmlComponent launcherComponent(&engine, QUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Shell/Bar/qml/LauncherSurface.qml")));
    auto *launcher = qobject_cast<QQuickWindow *>(launcherComponent.createWithInitialProperties({
        {QStringLiteral("barGeometry"), QVariant::fromValue(&metrics)},
        {QStringLiteral("popupController"), QVariant::fromValue(&popup)},
        {QStringLiteral("workspaceModel"), QVariant::fromValue(&model)},
    }));
    QVERIFY(launcher != nullptr);
    launcher->show();
    QTest::qWait(20);
    QCOMPARE(launcher->width(), launcher->property("launcherSurfaceWidth").toInt());
    QVERIFY(launcher->width() >= 176);
    const int reservedLauncherWidth = launcher->width();
    model.replaceWorkspaces({
        {QStringLiteral("1"), false, true, false, {}},
        {QStringLiteral("2"), false, false, false, {}},
        {QStringLiteral("3"), false, false, false, {}},
        {QStringLiteral("4"), true, false, false, {}},
    });
    QTRY_COMPARE_WITH_TIMEOUT(launcher->width(), reservedLauncherWidth, 500);
    delete launcher;
}

void BarQmlSmokeTest::workspaceActivationIsTruthfullyUnavailable()
{
    QQmlEngine engine;
    BarLayoutMetrics metrics;
    BarPopupController popup;
    WorkspaceModel model;
    model.replaceWorkspaces({
        {QStringLiteral("1"), true, true, false, {}, QStringLiteral("typhon.workspace.1")},
        {QStringLiteral("2"), false, false, false, {}, QStringLiteral("typhon.workspace.2")},
    });

    QQmlComponent component(
        &engine, QUrl(QStringLiteral(
            "qrc:/qt/qml/Astrea/Shell/Bar/qml/LauncherSurface.qml")));
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties({
        {QStringLiteral("barGeometry"), QVariant::fromValue(&metrics)},
        {QStringLiteral("popupController"), QVariant::fromValue(&popup)},
        {QStringLiteral("workspaceModel"), QVariant::fromValue(&model)},
    }));
    QVERIFY(window != nullptr);
    window->resize(240, 40);
    window->show();
    QTest::qWait(20);

    QObject *workspaceStrip = window->findChild<QObject *>(QStringLiteral("workspaceStrip"));
    QVERIFY(workspaceStrip != nullptr);
    QSignalSpy activationSpy(workspaceStrip, SIGNAL(workspaceActivated(QString)));
    QObject *repeater = window->findChild<QObject *>(QStringLiteral("workspaceRepeater"));
    QVERIFY(repeater != nullptr);
    QQuickItem *delegate = nullptr;
    QVERIFY(QMetaObject::invokeMethod(repeater, "itemAt", Qt::DirectConnection,
                                      Q_RETURN_ARG(QQuickItem *, delegate), Q_ARG(int, 1)));
    QVERIFY(delegate != nullptr);
    auto hitboxes = delegate->findChildren<QObject *>(QStringLiteral("workspaceHitTarget"));
    QCOMPARE(hitboxes.size(), 1);
    auto *hitbox = qobject_cast<QQuickItem *>(hitboxes.front());
    QVERIFY(hitbox != nullptr);
    const QPointF localPoint = hitbox->mapToItem(window->contentItem(),
                                                  QPointF(hitbox->width() / 2.0,
                                                          hitbox->height() / 2.0));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, localPoint.toPoint());
    QTest::qWait(50);
    QCOMPARE(activationSpy.count(), 0);
    delete window;
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

void BarQmlSmokeTest::volumeUiDisablesWhenDefaultStateUnavailable()
{
    QQmlEngine engine;
    FakeAudioService audio;
    QQmlComponent indicatorComponent(
        &engine, QUrl(QStringLiteral(
            "qrc:/qt/qml/Astrea/Shell/Bar/qml/components/VolumeIndicator.qml")));
    QObject *indicator = indicatorComponent.createWithInitialProperties({
        {QStringLiteral("audioService"), QVariant::fromValue(&audio)},
    });
    QVERIFY(indicator != nullptr);
    auto *indicatorItem = qobject_cast<QQuickItem *>(indicator);
    QVERIFY(indicatorItem != nullptr);
    QObject *wheel = indicator->findChild<QObject *>(QStringLiteral("volumeWheelArea"));
    QVERIFY(wheel != nullptr);
    QQuickWindow window;
    window.resize(100, 40);
    indicatorItem->setParentItem(window.contentItem());
    indicatorItem->setWidth(100);
    indicatorItem->setHeight(40);
    window.show();
    QTest::qWait(20);

    audio.setDefaultStateAvailableForTest(false);
    QCoreApplication::processEvents();
    QTest::wheelEvent(&window, QPointF(50, 20), QPoint(0, 120));
    QCoreApplication::processEvents();
    QCOMPARE(audio.m_lastDelta, 0.0);

    QQmlComponent popupComponent(
        &engine, QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Bar/qml/VolumePopup.qml")));
    QObject *popup = popupComponent.createWithInitialProperties({
        {QStringLiteral("audioService"), QVariant::fromValue(&audio)},
    });
    QVERIFY(popup != nullptr);
    QObject *muteMouse = popup->findChild<QObject *>(QStringLiteral("volumeMuteMouse"));
    QObject *sliderMouse = popup->findChild<QObject *>(QStringLiteral("volumeSliderMouse"));
    QVERIFY(muteMouse != nullptr);
    QVERIFY(sliderMouse != nullptr);
    QVERIFY(!muteMouse->property("enabled").toBool());
    QVERIFY(!sliderMouse->property("enabled").toBool());

    audio.setDefaultStateAvailableForTest(true);
    QCoreApplication::processEvents();
    QVERIFY(muteMouse->property("enabled").toBool());
    QVERIFY(sliderMouse->property("enabled").toBool());
    delete popup;
    delete indicator;
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication application(argc, argv);
    BarQmlSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "BarQmlSmokeTest.moc"
