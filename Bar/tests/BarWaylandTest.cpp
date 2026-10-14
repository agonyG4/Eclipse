#include "core/BarController.hpp"
#include "core/BarPopupController.hpp"
#include "platform/wayland/BarSurfaceBundle.hpp"
#include "platform/wayland/BarSurfaceManager.hpp"
#include "platform/wayland/BarSurfacePolicy.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QSignalSpy>
#include <QTest>

#include <utility>

class BarWaylandTest final : public QObject {
    Q_OBJECT

private slots:
    void reservePolicyUsesTransparentTopReservation();
    void launcherAndStatusPoliciesPreserveReferenceGeometry();
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

void BarWaylandTest::reservePolicyUsesTransparentTopReservation()
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

void BarWaylandTest::launcherAndStatusPoliciesPreserveReferenceGeometry()
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
    QCOMPARE(trayTooltip.margins, QMargins(0, 43, 0, 0));
    QCOMPARE(BarSurfacePolicy::surfaceHeight(BarSurfaceKind::TrayTooltip), 28);
}

void BarWaylandTest::surfaceManagerOwnsProductionLifecycleAndEnablement()
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

void BarWaylandTest::surfaceRemovalDuringPopupCloseIsImmediate()
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

void BarWaylandTest::surfaceManagerShutdownIsTerminal()
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

void BarWaylandTest::surfaceManagerUnwindsBundleCreationFailure()
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

void BarWaylandTest::popupOwnershipIsLocalToEachSurfaceBundle()
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
    BarWaylandTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "BarWaylandTest.moc"
