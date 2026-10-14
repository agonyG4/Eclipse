#include "services/BarClockService.hpp"
#include "core/BarController.hpp"
#include "core/BarLayoutMetrics.hpp"
#include "core/BarLayoutPolicy.hpp"
#include "core/BarPopupController.hpp"
#include "core/WorkspaceModel.hpp"
#include "apps/DesktopEntryCatalog.hpp"
#include "launch/ApplicationLauncher.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

class BarCoreTest final : public QObject {
    Q_OBJECT

private slots:
    void layoutPolicyPreservesReferenceGeometry();
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
    void searchCapabilityUsesPublicSignalContract();
    void settingsActionUsesCatalogAndLauncherSeam();
};




void BarCoreTest::layoutPolicyPreservesReferenceGeometry()
{
    QCOMPARE(BarLayoutPolicy::kBarHeight, 45);
    QCOMPARE(BarLayoutPolicy::kPillHeight, 36);
    QCOMPARE(BarLayoutPolicy::kLauncherLeftMargin, 8);
    QCOMPARE(BarLayoutPolicy::kPillTopMargin, 5);
    QCOMPARE(BarLayoutPolicy::kStatusRightMargin, 6);
    QCOMPARE(BarLayoutPolicy::kMinimumGap, 28);
    QCOMPARE(BarLayoutPolicy::kSidePadding, 10);
    QCOMPARE(BarLayoutPolicy::kPopupSidePadding, 8);
    QCOMPARE(BarLayoutPolicy::kPopupTopOffset, 54);
    QCOMPARE(BarLayoutPolicy::kTrayTooltipHeight, 28);
    QCOMPARE(BarLayoutPolicy::kTrayTooltipTopMargin, 43);
    QCOMPARE(BarLayoutPolicy::statusWidth(1280, 240, 1200), 998);
    QCOMPARE(BarLayoutPolicy::statusLeft(1280, 998), 276);
    QCOMPARE(BarLayoutPolicy::popupX(1920, 300, 20), 8);
    QCOMPARE(BarLayoutPolicy::popupX(1920, 300, 960), 810);
}

void BarCoreTest::statusWidthIsCappedBeforeLauncherGap()
{
    const int width = BarLayoutPolicy::statusWidth(1280, 240, 1200);
    const int left = BarLayoutPolicy::statusLeft(1280, width);
    QVERIFY(width <= 1280 - 8 - 240 - 28 - 6);
    QVERIFY(left - (8 + 240) >= 28);

    const int narrow = BarLayoutPolicy::statusWidth(1024, 1000, 800);
    QCOMPARE(narrow, 0);
    QVERIFY(BarLayoutPolicy::statusLeft(1024, narrow) > 0);
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
    QCOMPARE(BarLayoutPolicy::popupX(1920, 300, 20, 8), 8);
    QCOMPARE(BarLayoutPolicy::popupX(1920, 300, 960, 8), 810);
    QCOMPARE(BarLayoutPolicy::popupX(1920, 300, 1890, 8), 1612);
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

}

void BarCoreTest::searchCapabilityUsesPublicSignalContract()
{
    BarController bar(nullptr, nullptr);
    QSignalSpy capabilitiesSpy(&bar, &BarController::capabilitiesChanged);
    QSignalSpy searchSpy(&bar, &BarController::searchRequested);
    QVERIFY(capabilitiesSpy.isValid());
    QVERIFY(searchSpy.isValid());

    QVERIFY(!bar.searchAvailable());
    QVERIFY(!bar.showSearch());
    QCOMPARE(searchSpy.count(), 0);
    QCOMPARE(capabilitiesSpy.count(), 0);

    bar.setSearchAvailable(true);
    QVERIFY(bar.searchAvailable());
    QCOMPARE(capabilitiesSpy.count(), 1);
    bar.setSearchAvailable(true);
    QCOMPARE(capabilitiesSpy.count(), 1);

    QVERIFY(bar.showSearch());
    QCOMPARE(searchSpy.count(), 1);

    bar.setEnabled(false);
    QVERIFY(!bar.showSearch());
    QCOMPARE(searchSpy.count(), 1);

    bar.setEnabled(true);
    bar.setSearchAvailable(false);
    QVERIFY(!bar.searchAvailable());
    QVERIFY(!bar.showSearch());
    QCOMPARE(searchSpy.count(), 1);
    QCOMPARE(capabilitiesSpy.count(), 2);
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






int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication application(argc, argv);
    BarCoreTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "BarCoreTest.moc"
