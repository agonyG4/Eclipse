#include "core/BarClockService.hpp"
#include "core/BarController.hpp"
#include "core/BarLayoutMetrics.hpp"
#include "core/BarPopupController.hpp"
#include "core/BarSurfacePolicy.hpp"
#include "core/WorkspaceModel.hpp"
#include "apps/DesktopEntryCatalog.hpp"
#include "launch/ApplicationLauncher.hpp"
#include "Spotlight/core/SpotlightController.hpp"
#include "platform/runtime/SpotlightRuntimePaths.hpp"
#include "platform/wayland/BarSurfaceBundle.hpp"
#include "platform/wayland/BarSurfaceManager.hpp"

#include <QDateTime>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include <functional>
#include <memory>

class BarCoreTest final : public QObject {
    Q_OBJECT

private slots:
    void reservePolicyUsesTransparentTopReservation();
    void launcherAndStatusPoliciesPreserveReferenceGeometry();
    void statusWidthIsCappedBeforeLauncherGap();
    void layoutMetricsAreSingleGeometryAuthority();
    void trayAnchorPreservesOutputAndSurfaceCoordinates();
    void popupPositionIsClampedToOutputPadding();
    void workspaceModelSortsAndExposesStableRoles();
    void workspaceModelAcceptsEmptyProductionData();
    void clockFormattingIsDeterministic();
    void clockSchedulesTheNextMinuteBoundary();
    void popupReplacesAndClearsOutputLocalState();
    void popupCloseRetainsRenderedStateUntilAnimationCompletes();
    void popupReopenCancelsClosingTransition();
    void popupRejectsUnsupportedKinds();
    void popupSupportsNativeIndicatorKinds();
    void spotlightComponentEnablementIsAuthoritative();
    void barSearchCapabilityTracksSpotlightEnablement();
    void settingsActionUsesCatalogAndLauncherSeam();
    void surfaceManagerOwnsProductionLifecycleAndEnablement();
    void surfaceRemovalDuringPopupCloseIsImmediate();
    void surfaceManagerShutdownIsTerminal();
    void surfaceManagerUnwindsBundleCreationFailure();
    void popupOwnershipIsLocalToEachSurfaceBundle();
};

namespace {

struct BundleCounters {
    int created = 0;
    int initialized = 0;
    int mapped = 0;
    int enabledChanges = 0;
    int geometryUpdates = 0;
    int destroyed = 0;
    bool lastEnabled = true;
};

class RecordingBundle final : public BarSurfaceBundle {
public:
    RecordingBundle(QScreen *screen, BundleCounters *counters, QObject *parent,
                    bool initializeSuccessfully = true)
        : BarSurfaceBundle(screen, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                           parent)
        , m_counters(counters)
        , m_initializeSuccessfully(initializeSuccessfully)
    {
        ++m_counters->created;
    }

    ~RecordingBundle() override
    {
        ++m_counters->destroyed;
    }

    bool initialize(QString *errorOut = nullptr) override
    {
        ++m_counters->initialized;
        if (!m_initializeSuccessfully && errorOut)
            *errorOut = QStringLiteral("injected bundle initialization failure");
        return m_initializeSuccessfully;
    }

    void map() override
    {
        ++m_counters->mapped;
    }

    void setBarEnabled(bool enabled) override
    {
        if (m_counters->lastEnabled == enabled)
            return;
        ++m_counters->enabledChanges;
        m_counters->lastEnabled = enabled;
        if (!enabled)
            popupController()->clearForOutput();
        else
            ++m_counters->mapped;
    }

    void updateForScreen() override
    {
        ++m_counters->geometryUpdates;
    }

private:
    BundleCounters *m_counters = nullptr;
    bool m_initializeSuccessfully = true;
};

} // namespace

void BarCoreTest::reservePolicyUsesTransparentTopReservation()
{
    const auto policy = BarSurfacePolicy::reserve();
    QCOMPARE(policy.scope, QStringLiteral("astrea-bar-reserve"));
    QCOMPARE(policy.layer, AstreaLayerShellConfig::Layer::Top);
    QCOMPARE(policy.keyboardInteractivity,
             AstreaLayerShellConfig::KeyboardInteractivity::None);
    QVERIFY(policy.anchorTop);
    QVERIFY(policy.anchorLeft);
    QVERIFY(policy.anchorRight);
    QVERIFY(!policy.anchorBottom);
    QCOMPARE(policy.exclusiveZone, 45);
    QCOMPARE(policy.margins, QMargins());
    QCOMPARE(BarSurfacePolicy::surfaceHeight(BarSurfaceKind::Reserve), 45);
}

void BarCoreTest::launcherAndStatusPoliciesPreserveReferenceGeometry()
{
    const auto launcher = BarSurfacePolicy::launcher();
    QCOMPARE(launcher.scope, QStringLiteral("astrea-bar-launcher"));
    QCOMPARE(launcher.layer, AstreaLayerShellConfig::Layer::Top);
    QVERIFY(launcher.anchorTop);
    QVERIFY(launcher.anchorLeft);
    QVERIFY(!launcher.anchorRight);
    QCOMPARE(launcher.exclusiveZone, -1);
    QCOMPARE(launcher.margins, QMargins(8, 5, 0, 0));
    QCOMPARE(BarSurfacePolicy::surfaceHeight(BarSurfaceKind::Launcher), 36);

    const auto status = BarSurfacePolicy::status();
    QCOMPARE(status.scope, QStringLiteral("astrea-bar-status"));
    QCOMPARE(status.layer, AstreaLayerShellConfig::Layer::Top);
    QVERIFY(status.anchorTop);
    QVERIFY(status.anchorRight);
    QVERIFY(!status.anchorLeft);
    QCOMPARE(status.exclusiveZone, -1);
    QCOMPARE(status.margins, QMargins(0, 5, 6, 0));
    QCOMPARE(BarSurfacePolicy::surfaceHeight(BarSurfaceKind::Status), 36);

    const auto popup = BarSurfacePolicy::popupOverlay();
    QCOMPARE(popup.scope, QStringLiteral("astrea-bar-popup"));
    QCOMPARE(popup.layer, AstreaLayerShellConfig::Layer::Overlay);
    QVERIFY(popup.anchorTop);
    QVERIFY(popup.anchorBottom);
    QVERIFY(popup.anchorLeft);
    QVERIFY(popup.anchorRight);
    QCOMPARE(popup.exclusiveZone, -1);
    QCOMPARE(popup.margins, QMargins());

    const auto trayTooltip = BarSurfacePolicy::trayTooltip();
    QCOMPARE(trayTooltip.scope, QStringLiteral("astrea-bar-tray-tooltip"));
    QCOMPARE(trayTooltip.layer, AstreaLayerShellConfig::Layer::Top);
    QVERIFY(trayTooltip.anchorTop);
    QVERIFY(trayTooltip.anchorLeft);
    QVERIFY(trayTooltip.anchorRight);
    QCOMPARE(trayTooltip.exclusiveZone, -1);
    QCOMPARE(trayTooltip.margins, QMargins(0, 51, 0, 0));
    QCOMPARE(BarSurfacePolicy::surfaceHeight(BarSurfaceKind::TrayTooltip), 28);
}

void BarCoreTest::statusWidthIsCappedBeforeLauncherGap()
{
    const int width = BarSurfacePolicy::statusWidth(1280, 240, 1200);
    const int left = BarSurfacePolicy::statusLeft(1280, width);
    QVERIFY(width <= 1280 - 8 - 240 - 28 - 6);
    QVERIFY(left - (8 + 240) >= 28);

    const int narrow = BarSurfacePolicy::statusWidth(1024, 1000, 800);
    QCOMPARE(narrow, 0);
    QVERIFY(BarSurfacePolicy::statusLeft(1024, narrow) > 0);
}

void BarCoreTest::layoutMetricsAreSingleGeometryAuthority()
{
    BarLayoutMetrics metrics;

    QCOMPARE(metrics.statusWidth(800, 100, 128), 128);
    QCOMPARE(metrics.statusWidth(800, 100, 1200), 658);
    QCOMPARE(metrics.statusWidth(180, 160, 120), 0);
    QCOMPARE(metrics.statusLeft(800, 128), 666);
    QCOMPARE(metrics.statusAnchorX(800, 128, 64), 730);

    QCOMPARE(metrics.popupWidth(100, 220), 84);
    QCOMPARE(metrics.popupX(1920, 300, 20), 8);
    QCOMPARE(metrics.popupX(1920, 300, 960), 810);
    QCOMPARE(metrics.popupX(100, 220, 96), 8);
    QCOMPARE(metrics.statusLeft(1, 0), 0);
    QCOMPARE(metrics.popupWidth(10, 220), 0);
    QCOMPARE(metrics.popupX(10, 220, 5), 8);
}

void BarCoreTest::trayAnchorPreservesOutputAndSurfaceCoordinates()
{
    BarLayoutMetrics metrics;
    const QVariantMap anchor = metrics.trayAnchor(1920, 1080, 1400, 5, 24, 18);
    QCOMPARE(anchor.value(QStringLiteral("localX")).toInt(), 1424);
    QCOMPARE(anchor.value(QStringLiteral("localY")).toInt(), 23);
    QCOMPARE(anchor.value(QStringLiteral("globalX")).toInt(), 3344);
    QCOMPARE(anchor.value(QStringLiteral("globalY")).toInt(), 1103);
}

void BarCoreTest::popupPositionIsClampedToOutputPadding()
{
    QCOMPARE(BarSurfacePolicy::popupX(1920, 300, 20, 8), 8);
    QCOMPARE(BarSurfacePolicy::popupX(1920, 300, 960, 8), 810);
    QCOMPARE(BarSurfacePolicy::popupX(1920, 300, 1890, 8), 1612);
}

void BarCoreTest::workspaceModelSortsAndExposesStableRoles()
{
    WorkspaceModel model;
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    model.replaceWorkspaces({
        {QStringLiteral("10"), false, true, false, QStringLiteral("DP-2")},
        {QStringLiteral("2"), true, true, false, QStringLiteral("DP-1")},
        {QStringLiteral("1"), false, false, true, QStringLiteral("DP-1")},
    });

    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.data(model.index(0, 0), WorkspaceModel::IdRole).toString(),
             QStringLiteral("1"));
    QCOMPARE(model.data(model.index(0, 0), WorkspaceModel::ActiveRole).toBool(), false);
    QCOMPARE(model.data(model.index(0, 0), WorkspaceModel::OccupiedRole).toBool(), false);
    QCOMPARE(model.data(model.index(0, 0), WorkspaceModel::UrgentRole).toBool(), true);
    QCOMPARE(model.data(model.index(0, 0), WorkspaceModel::OutputIdRole).toString(),
             QStringLiteral("DP-1"));
    QCOMPARE(model.roleNames().value(WorkspaceModel::IdRole), QByteArrayLiteral("id"));
    QCOMPARE(model.roleNames().value(WorkspaceModel::OutputIdRole),
             QByteArrayLiteral("outputId"));
}

void BarCoreTest::workspaceModelAcceptsEmptyProductionData()
{
    WorkspaceModel model;
    model.replaceWorkspaces({});
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(model.workspaceItems().isEmpty());
}

void BarCoreTest::clockFormattingIsDeterministic()
{
    const QDateTime timestamp(QDate(2026, 8, 18), QTime(17, 4, 12, 250), QTimeZone::UTC);
    const QLocale locale(QLocale::English, QLocale::UnitedStates);
    QCOMPARE(BarClockService::formatTime(timestamp, locale,
                                         BarClockService::ClockTimeFormat::TwentyFourHour),
             QStringLiteral("17:04"));
    QCOMPARE(BarClockService::formatTime(timestamp, locale,
                                         BarClockService::ClockTimeFormat::TwelveHour),
             QStringLiteral("05:04 PM"));
    QCOMPARE(BarClockService::formatDate(timestamp, locale), QStringLiteral("Tue Aug 18"));
}

void BarCoreTest::clockSchedulesTheNextMinuteBoundary()
{
    const QDateTime timestamp(QDate(2026, 8, 18), QTime(17, 4, 12, 250), QTimeZone::UTC);
    QCOMPARE(BarClockService::nextMinuteDelayMs(timestamp), 47770);
}

void BarCoreTest::popupReplacesAndClearsOutputLocalState()
{
    BarPopupController controller;
    QSignalSpy changedSpy(&controller, &BarPopupController::changed);

    QVERIFY(!controller.isOpen());
    controller.open(BarPopupController::PopupKind::AstreaMenu, 120);
    QVERIFY(controller.isOpen());
    QCOMPARE(controller.kind(), BarPopupController::PopupKind::AstreaMenu);
    QCOMPARE(controller.anchorX(), 120);

    const auto unsupported = static_cast<BarPopupController::PopupKind>(2);
    controller.open(unsupported, 1770);
    QVERIFY(controller.isOpen());
    QCOMPARE(controller.kind(), BarPopupController::PopupKind::AstreaMenu);
    QCOMPARE(controller.anchorX(), 120);

    controller.close();
    controller.close();
    QVERIFY(!controller.isOpen());
    controller.open(BarPopupController::PopupKind::AstreaMenu, 42);
    controller.clearForOutput();
    QVERIFY(!controller.isOpen());
    QVERIFY(changedSpy.count() >= 4);
}

void BarCoreTest::popupCloseRetainsRenderedStateUntilAnimationCompletes()
{
    BarPopupController controller;
    controller.open(BarPopupController::PopupKind::AstreaMenu, 900);

    controller.close();

    QVERIFY(!controller.isOpen());
    QVERIFY(controller.closing());
    QVERIFY(controller.surfaceRequired());
    QCOMPARE(controller.kind(), BarPopupController::PopupKind::AstreaMenu);
    QCOMPARE(controller.anchorX(), 900);

    controller.completeClose();

    QVERIFY(!controller.isOpen());
    QVERIFY(!controller.closing());
    QVERIFY(!controller.surfaceRequired());
    QCOMPARE(controller.kind(), BarPopupController::PopupKind::None);
    QCOMPARE(controller.anchorX(), 0);
}

void BarCoreTest::popupReopenCancelsClosingTransition()
{
    BarPopupController controller;
    controller.open(BarPopupController::PopupKind::AstreaMenu, 900);
    controller.close();

    controller.toggleAstreaMenu(920);

    QVERIFY(controller.isOpen());
    QVERIFY(!controller.closing());
    QVERIFY(controller.surfaceRequired());
    QCOMPARE(controller.kind(), BarPopupController::PopupKind::AstreaMenu);
    QCOMPARE(controller.anchorX(), 920);

    controller.close();
    controller.open(BarPopupController::PopupKind::AstreaMenu, 160);
    QVERIFY(controller.isOpen());
    QCOMPARE(controller.kind(), BarPopupController::PopupKind::AstreaMenu);
    QCOMPARE(controller.anchorX(), 160);
}

void BarCoreTest::popupRejectsUnsupportedKinds()
{
    BarPopupController controller;
    const auto unsupported = static_cast<BarPopupController::PopupKind>(2);

    controller.open(unsupported, 300);
    QVERIFY(!controller.isOpen());
    QVERIFY(!controller.surfaceRequired());
    QCOMPARE(controller.kind(), BarPopupController::PopupKind::None);

    controller.toggle(unsupported, 300);
    QVERIFY(!controller.isOpen());
    QVERIFY(!controller.surfaceRequired());
    QCOMPARE(controller.kind(), BarPopupController::PopupKind::None);
}

void BarCoreTest::popupSupportsNativeIndicatorKinds()
{
    BarPopupController controller;

    controller.toggleNetwork(240);
    QCOMPARE(controller.kind(), BarPopupController::PopupKind::Network);
    QCOMPARE(controller.anchorX(), 240);
    QVERIFY(controller.isOpen());

    controller.toggleBluetooth(280);
    QCOMPARE(controller.kind(), BarPopupController::PopupKind::Bluetooth);
    QCOMPARE(controller.anchorX(), 280);

    controller.toggleVolume(320);
    QCOMPARE(controller.kind(), BarPopupController::PopupKind::Volume);
    QCOMPARE(controller.anchorX(), 320);

    controller.toggleTrayMenu(420, QStringLiteral("org.example.Tray|/StatusNotifierItem"));
    QCOMPARE(controller.kind(), BarPopupController::PopupKind::TrayMenu);
    QCOMPARE(controller.contextKey(), QStringLiteral("org.example.Tray|/StatusNotifierItem"));

    controller.close();
    QVERIFY(controller.closing());
    QVERIFY(controller.surfaceRequired());
    QCOMPARE(controller.kind(), BarPopupController::PopupKind::TrayMenu);
    controller.completeClose();
    QVERIFY(!controller.surfaceRequired());
    QCOMPARE(controller.kind(), BarPopupController::PopupKind::None);
}

void BarCoreTest::spotlightComponentEnablementIsAuthoritative()
{
    const SpotlightRuntimePaths paths = SpotlightRuntimePaths::fromEnvironment();
    SpotlightController spotlight(paths);
    spotlight.setWeatherEnabled(false);
    spotlight.setComponentEnabled(false);

    QVERIFY(!spotlight.componentEnabled());
    spotlight.show();
    QVERIFY(!spotlight.isOpen());
    spotlight.toggle();
    QVERIFY(!spotlight.isOpen());
    spotlight.setQuery(QStringLiteral("settings"));
    QVERIFY(!spotlight.isOpen());

    spotlight.setComponentEnabled(true);
    QVERIFY(spotlight.componentEnabled());
    spotlight.show();
    QVERIFY(spotlight.isOpen());
}

void BarCoreTest::barSearchCapabilityTracksSpotlightEnablement()
{
    const SpotlightRuntimePaths paths = SpotlightRuntimePaths::fromEnvironment();
    SpotlightController spotlight(paths);
    spotlight.setWeatherEnabled(false);
    BarController missing(nullptr, nullptr, nullptr);
    QVERIFY(!missing.searchAvailable());
    QVERIFY(!missing.showSearch());

    BarController bar(nullptr, nullptr, &spotlight);
    QSignalSpy capabilitiesSpy(&bar, &BarController::capabilitiesChanged);

    QVERIFY(bar.searchAvailable());
    QVERIFY(bar.showSearch());
    QVERIFY(spotlight.isOpen());

    spotlight.setComponentEnabled(false);
    QVERIFY(!bar.searchAvailable());
    QVERIFY(capabilitiesSpy.count() >= 1);
    QVERIFY(!bar.showSearch());
    QVERIFY(!spotlight.isOpen());

    spotlight.setComponentEnabled(true);
    QVERIFY(bar.searchAvailable());
    QVERIFY(bar.showSearch());
    QVERIFY(spotlight.isOpen());
}

void BarCoreTest::settingsActionUsesCatalogAndLauncherSeam()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString applications = directory.filePath(
        QStringLiteral(".local/share/applications"));
    QVERIFY(QDir().mkpath(applications));
    QFile desktopFile(QDir(applications).filePath(QStringLiteral("astrea-settings.desktop")));
    QVERIFY(desktopFile.open(QIODevice::WriteOnly | QIODevice::Text));
    desktopFile.write("[Desktop Entry]\nType=Application\nName=Astrea Settings\n"
                     "Exec=astrea-settings\nIcon=preferences-system\n");
    desktopFile.close();

    class RecordingLauncher final : public ApplicationLauncher {
    public:
        RecordingLauncher()
            : ApplicationLauncher(QStringLiteral("/does/not/launch"))
        {
        }

        void launchDesktop(const ApplicationLaunchRequest &request) override
        {
            requests.append(request);
        }

        QList<ApplicationLaunchRequest> requests;
    } launcher;

    DesktopEntryCatalog catalog;
    BarController bar(&catalog, &launcher, nullptr);
    QSignalSpy capabilitiesSpy(&bar, &BarController::capabilitiesChanged);

    QVERIFY(!bar.settingsAvailable());
    catalog.initialize(directory.path());
    QVERIFY(bar.settingsAvailable());
    QVERIFY(capabilitiesSpy.count() >= 1);
    QVERIFY(bar.launchSettings());
    QCOMPARE(launcher.requests.size(), 1);
    QCOMPARE(launcher.requests.constFirst().desktopFileName,
             QStringLiteral("astrea-settings.desktop"));

    bar.setEnabled(false);
    QVERIFY(!bar.launchSettings());
    QCOMPARE(launcher.requests.size(), 1);
}

void BarCoreTest::surfaceManagerOwnsProductionLifecycleAndEnablement()
{
    auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    QScreen *screen = QGuiApplication::primaryScreen();
    QVERIFY(screen != nullptr);
    QQmlApplicationEngine engine;
    BarController bar(nullptr, nullptr, nullptr);
    BundleCounters counters;
    BarSurfaceManager::BundleFactory factory = [&counters](QScreen *output, QObject *parent) {
        return new RecordingBundle(output, &counters, parent);
    };
    BarSurfaceManager manager(*application, engine, &bar, nullptr, nullptr,
                              nullptr, nullptr, nullptr, nullptr, std::move(factory));
    QSignalSpy countSpy(&manager, &BarSurfaceManager::bundleCountChanged);

    QVERIFY(manager.initialize());
    QCOMPARE(manager.bundleCount(), 1);
    QCOMPARE(counters.created, 1);
    QCOMPARE(counters.initialized, 1);
    QCOMPARE(counters.mapped, 1);

    application->screenAdded(screen);
    QCOMPARE(manager.bundleCount(), 1);
    QCOMPARE(counters.created, 1);

    auto *first = qobject_cast<BarSurfaceBundle *>(manager.findChild<BarSurfaceBundle *>());
    QVERIFY(first != nullptr);
    QPointer<BarSurfaceBundle> firstGuard(first);
    first->popupController()->open(BarPopupController::PopupKind::AstreaMenu, 300);
    QVERIFY(manager.popupOpen());

    screen->geometryChanged(screen->geometry());
    QCOMPARE(counters.geometryUpdates, 1);

    bar.setEnabled(false);
    QCOMPARE(counters.lastEnabled, false);
    QVERIFY(!manager.popupOpen());
    bar.setEnabled(true);
    QCOMPARE(counters.lastEnabled, true);
    QCOMPARE(counters.mapped, 2);

    application->screenRemoved(screen);
    QCOMPARE(manager.bundleCount(), 0);
    QCOMPARE(counters.destroyed, 1);
    QVERIFY(!manager.popupOpen());
    QVERIFY(firstGuard.isNull());

    application->screenAdded(screen);
    QCOMPARE(manager.bundleCount(), 1);
    QCOMPARE(counters.created, 2);
    QVERIFY(manager.findChild<BarSurfaceBundle *>() != nullptr);

    manager.shutdown();
    QCOMPARE(manager.bundleCount(), 0);
    QCOMPARE(counters.destroyed, 2);
    const int signalCount = countSpy.count();
    manager.shutdown();
    QCOMPARE(counters.destroyed, 2);
    QCOMPARE(countSpy.count(), signalCount);
}

void BarCoreTest::surfaceRemovalDuringPopupCloseIsImmediate()
{
    auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    QScreen *screen = QGuiApplication::primaryScreen();
    QVERIFY(screen != nullptr);
    QQmlApplicationEngine engine;
    BarController bar(nullptr, nullptr, nullptr);
    BundleCounters counters;
    BarSurfaceManager::BundleFactory factory = [&counters](QScreen *output, QObject *parent) {
        return new RecordingBundle(output, &counters, parent);
    };
    BarSurfaceManager manager(*application, engine, &bar, nullptr, nullptr,
                              nullptr, nullptr, nullptr, nullptr, std::move(factory));

    QVERIFY(manager.initialize());
    BarSurfaceBundle *bundle = nullptr;
    for (BarSurfaceBundle *candidate : manager.findChildren<BarSurfaceBundle *>()) {
        if (candidate && candidate->screen() == screen) {
            bundle = candidate;
            break;
        }
    }
    QVERIFY(bundle != nullptr);
    const int initializedBundleCount = manager.bundleCount();
    bundle->popupController()->open(BarPopupController::PopupKind::AstreaMenu, 500);
    bundle->popupController()->close();
    QVERIFY(bundle->popupController()->closing());

    application->screenRemoved(screen);

    QCOMPARE(manager.bundleCount(), initializedBundleCount - 1);
    QCOMPARE(counters.destroyed, 1);
    QVERIFY(!manager.popupOpen());
}

void BarCoreTest::surfaceManagerShutdownIsTerminal()
{
    auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    QScreen *screen = QGuiApplication::primaryScreen();
    QVERIFY(screen != nullptr);
    QQmlApplicationEngine engine;
    BarController bar(nullptr, nullptr, nullptr);
    BundleCounters counters;
    BarSurfaceManager::BundleFactory factory = [&counters](QScreen *output, QObject *parent) {
        return new RecordingBundle(output, &counters, parent);
    };
    BarSurfaceManager manager(*application, engine, &bar, nullptr, nullptr,
                              nullptr, nullptr, nullptr, nullptr, std::move(factory));
    QSignalSpy countSpy(&manager, &BarSurfaceManager::bundleCountChanged);
    QSignalSpy popupSpy(&manager, &BarSurfaceManager::popupStateChanged);
    QSignalSpy layerSpy(&manager, &BarSurfaceManager::layerStateChanged);

    QVERIFY(manager.initialize());
    const int initializedBundleCount = manager.bundleCount();
    QCOMPARE(counters.created, initializedBundleCount);
    manager.shutdown();
    QCOMPARE(manager.bundleCount(), 0);
    QCOMPARE(counters.destroyed, initializedBundleCount);
    const int countSignals = countSpy.count();
    const int popupSignals = popupSpy.count();
    const int layerSignals = layerSpy.count();

    application->screenAdded(screen);
    application->screenRemoved(screen);
    screen->geometryChanged(screen->geometry());
    screen->availableGeometryChanged(screen->availableGeometry());
    bar.setEnabled(false);
    bar.setEnabled(true);

    QCOMPARE(manager.bundleCount(), 0);
    QCOMPARE(counters.created, initializedBundleCount);
    QCOMPARE(counters.destroyed, initializedBundleCount);
    QCOMPARE(counters.geometryUpdates, 0);
    QCOMPARE(countSpy.count(), countSignals);
    QCOMPARE(popupSpy.count(), popupSignals);
    QCOMPARE(layerSpy.count(), layerSignals);

    QString error;
    QVERIFY(!manager.initialize(&error));
    QVERIFY(error.contains(QStringLiteral("shut down"), Qt::CaseInsensitive));
    manager.shutdown();
    QCOMPARE(counters.destroyed, initializedBundleCount);
    QCOMPARE(countSpy.count(), countSignals);
    QCOMPARE(popupSpy.count(), popupSignals);
    QCOMPARE(layerSpy.count(), layerSignals);
}

void BarCoreTest::surfaceManagerUnwindsBundleCreationFailure()
{
    auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    QVERIFY(application != nullptr);
    QScreen *screen = QGuiApplication::primaryScreen();
    QVERIFY(screen != nullptr);
    QQmlApplicationEngine engine;
    BarController bar(nullptr, nullptr, nullptr);
    BundleCounters counters;
    BarSurfaceManager::BundleFactory factory = [&counters](QScreen *output, QObject *parent) {
        return new RecordingBundle(output, &counters, parent, false);
    };
    BarSurfaceManager manager(*application, engine, &bar, nullptr, nullptr,
                              nullptr, nullptr, nullptr, nullptr, std::move(factory));

    QString error;
    QVERIFY(!manager.initialize(&error));
    QVERIFY(error.contains(QStringLiteral("injected")));
    QCOMPARE(manager.bundleCount(), 0);
    QCOMPARE(counters.created, 1);
    QCOMPARE(counters.initialized, 1);
    QCOMPARE(counters.destroyed, 1);
    manager.shutdown();
    QCOMPARE(counters.destroyed, 1);
}

void BarCoreTest::popupOwnershipIsLocalToEachSurfaceBundle()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    QVERIFY(screen != nullptr);
    BarSurfaceBundle first(screen, nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr, nullptr);
    BarSurfaceBundle second(screen, nullptr, nullptr, nullptr, nullptr,
                            nullptr, nullptr, nullptr);

    first.popupController()->open(BarPopupController::PopupKind::AstreaMenu, 200);
    QVERIFY(first.popupOpen());
    QVERIFY(!second.popupOpen());

    first.destroySurfaces();
    QVERIFY(!first.popupOpen());
    QVERIFY(!second.popupOpen());
    second.popupController()->open(BarPopupController::PopupKind::AstreaMenu, 700);
    QVERIFY(second.popupOpen());
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication application(argc, argv);
    BarCoreTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "BarCoreTest.moc"
