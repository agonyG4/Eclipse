#include <QTest>

#include "apps/DesktopEntryCatalog.hpp"
#include "platform/typhon/DockApplicationStateProjector.hpp"
#include "platform/typhon/RuntimeTaskIdentityTracker.hpp"

using namespace Astrea::Typhon;

namespace {

std::shared_ptr<DesktopEntrySnapshot> catalog()
{
    auto result = std::make_shared<DesktopEntrySnapshot>();
    const auto add = [result](const QString &fileName, const QString &id) {
        DesktopEntryRecord entry;
        entry.desktopFileName = fileName;
        entry.id = id;
        result->byDesktopFileName.insert(fileName, result->entries.size());
        result->byDesktopId.insert(id, result->entries.size());
        result->entries.append(entry);
    };
    add(QStringLiteral("one.desktop"), QStringLiteral("one"));
    add(QStringLiteral("two.desktop"), QStringLiteral("two"));
    return result;
}

Toplevel window(const QString &id, const QString &appId, bool active = false,
                bool minimized = false, quint32 pid = 1, FocusSerial focusSerial = 0)
{
    Toplevel result;
    result.id = id;
    result.appId = appId;
    result.title = id;
    result.pid = pid;
    result.focusSerial = focusSerial;
    result.states = minimized ? ToplevelStates(ToplevelStateFlag::Minimized) : ToplevelStates{};
    if (active)
        result.states |= ToplevelStateFlag::Active;
    return result;
}

Snapshot snapshot(std::initializer_list<Toplevel> windows)
{
    Snapshot result;
    result.windows = QVector<Toplevel>(windows);
    result.total = static_cast<quint32>(result.windows.size());
    return result;
}

Snapshot snapshot(quint64 generation, std::initializer_list<Toplevel> windows)
{
    Snapshot result = snapshot(windows);
    result.connectionGeneration = generation;
    return result;
}

std::shared_ptr<DesktopEntrySnapshot> lateCatalog()
{
    auto result = catalog();
    DesktopEntryRecord entry;
    entry.desktopFileName = QStringLiteral("late.desktop");
    entry.id = QStringLiteral("late-app");
    entry.name = QStringLiteral("Late App");
    entry.icon = QStringLiteral("late-icon");
    const int index = result->entries.size();
    result->entries.append(entry);
    result->byDesktopFileName.insert(entry.desktopFileName, index);
    result->byDesktopId.insert(entry.id, index);
    return result;
}

} // namespace

class DockApplicationStateProjectorTest final : public QObject {
    Q_OBJECT

private slots:
    void zeroWindowsProducesClearedPinnedState();
    void nonPinnedApplicationsAreProjected();
    void runningAndActiveStateIsProjected();
    void minimizedWindowsRemainRunning();
    void duplicatePidsRemainSeparateAndOrdered();
    void unresolvedSteamAppIdProducesRuntimeTask();
    void equalAppIdsGroupIntoOneTask();
    void appIdsGroupCaseInsensitively();
    void appIdPunctuationRemainsOpaque();
    void emptyAppIdFallsBackToWindowId();
    void minimizedUnresolvedWindowRemainsRunning();
    void matchedLauncherUsesDesktopTaskKey();
    void aWindowContributesToOnlyOneGroup();
    void encounterOrderIsUniqueAndDeterministic();
    void focusSerialOrdersExactActivationTargets();
    void countsAreClampedToInt();
    void stress100ProjectionCycles();
    void catalogGainKeepsLiveTaskKey();
    void catalogLossKeepsLiveTaskKey();
    void metadataChangeKeepsLiveTaskKey();
    void metadataChangeAliasesNewWindowIntoExistingTask();
    void metadataChangeAliasIsIndependentOfWindowOrder();
    void newWindowJoinsExistingAppCohort();
    void generationChangeStartsFreshTaskLifetime();
    void authorityResetStartsFreshTaskLifetime();
    void emptyAppIdWindowsRemainSeparate();
};

void DockApplicationStateProjectorTest::zeroWindowsProducesClearedPinnedState()
{
    DockApplicationStateProjector projector;
    const auto projection = projector.project(snapshot({}), catalog());
    QVERIFY(projection.states.isEmpty());
    QVERIFY(projection.encounterOrder.isEmpty());
}

void DockApplicationStateProjectorTest::nonPinnedApplicationsAreProjected()
{
    DockApplicationStateProjector projector;
    const auto projection = projector.project(
        snapshot({window(QStringLiteral("1"), QStringLiteral("two"))}), catalog());

    QVERIFY(projection.states.contains(QStringLiteral("desktop:two.desktop")));
    QCOMPARE(projection.encounterOrder, QStringList{QStringLiteral("desktop:two.desktop")});
    QVERIFY(projection.states.value(QStringLiteral("desktop:two.desktop")).running);
}

void DockApplicationStateProjectorTest::runningAndActiveStateIsProjected()
{
    DockApplicationStateProjector projector;
    const auto projection = projector.project(
        snapshot({window(QStringLiteral("1"), QStringLiteral("one"), true)}),
        catalog());
    const auto state = projection.states.value(QStringLiteral("desktop:one.desktop"));
    QVERIFY(state.running);
    QVERIFY(state.active);
    QCOMPARE(state.windowCount, 1);
    QCOMPARE(state.windowIds, QVector<QString>{QStringLiteral("1")});
}

void DockApplicationStateProjectorTest::minimizedWindowsRemainRunning()
{
    DockApplicationStateProjector projector;
    const auto projection = projector.project(
        snapshot({window(QStringLiteral("1"), QStringLiteral("one"), false, true)}),
        catalog());
    QVERIFY(projection.states.value(QStringLiteral("desktop:one.desktop")).running);
    QVERIFY(!projection.states.value(QStringLiteral("desktop:one.desktop")).active);
}

void DockApplicationStateProjectorTest::duplicatePidsRemainSeparateAndOrdered()
{
    DockApplicationStateProjector projector;
    const auto projection = projector.project(
        snapshot({window(QStringLiteral("2"), QStringLiteral("one"), false, false, 4),
                  window(QStringLiteral("1"), QStringLiteral("one"), true, false, 4)}),
        catalog());
    QCOMPARE(projection.states.value(QStringLiteral("desktop:one.desktop")).windowCount, 2);
    const QVector<QString> expected{QStringLiteral("2"), QStringLiteral("1")};
    QCOMPARE(projection.states.value(QStringLiteral("desktop:one.desktop")).windowIds, expected);
}

void DockApplicationStateProjectorTest::unresolvedSteamAppIdProducesRuntimeTask()
{
    DockApplicationStateProjector projector;
    const auto projection = projector.project(
        snapshot({window(QStringLiteral("42"), QStringLiteral("steam_app_1091500"))}), catalog());

    const QString key = QStringLiteral("app:steam_app_1091500");
    QVERIFY(projection.states.contains(key));
    QCOMPARE(projection.encounterOrder, QStringList{key});
    QCOMPARE(projection.states.value(key).windowIds, QVector<QString>{QStringLiteral("42")});
}

void DockApplicationStateProjectorTest::equalAppIdsGroupIntoOneTask()
{
    DockApplicationStateProjector projector;
    const auto projection = projector.project(
        snapshot({window(QStringLiteral("1"), QStringLiteral("runtime-app")),
                  window(QStringLiteral("2"), QStringLiteral("runtime-app"))}), catalog());

    const QString key = QStringLiteral("app:runtime-app");
    QCOMPARE(projection.states.value(key).windowCount, 2);
    QCOMPARE(projection.encounterOrder, QStringList{key});
}

void DockApplicationStateProjectorTest::appIdsGroupCaseInsensitively()
{
    DockApplicationStateProjector projector;
    const auto projection = projector.project(
        snapshot({window(QStringLiteral("1"), QStringLiteral("Steam_App_1091500")),
                  window(QStringLiteral("2"), QStringLiteral("steam_app_1091500"))}), catalog());

    const QString key = QStringLiteral("app:steam_app_1091500");
    QCOMPARE(projection.states.value(key).windowCount, 2);
    QCOMPARE(projection.states.size(), 1);
}

void DockApplicationStateProjectorTest::appIdPunctuationRemainsOpaque()
{
    DockApplicationStateProjector projector;
    const auto projection = projector.project(
        snapshot({window(QStringLiteral("1"), QStringLiteral("steam_app_1")),
                  window(QStringLiteral("2"), QStringLiteral("steam-app-1"))}), catalog());

    QVERIFY(projection.states.contains(QStringLiteral("app:steam_app_1")));
    QVERIFY(projection.states.contains(QStringLiteral("app:steam-app-1")));
    QCOMPARE(projection.states.size(), 2);
}

void DockApplicationStateProjectorTest::emptyAppIdFallsBackToWindowId()
{
    DockApplicationStateProjector projector;
    const auto projection = projector.project(
        snapshot({window(QStringLiteral("42"), QString())}), catalog());

    const QString key = QStringLiteral("window:42");
    QVERIFY(projection.states.contains(key));
    QCOMPARE(projection.encounterOrder, QStringList{key});
}

void DockApplicationStateProjectorTest::minimizedUnresolvedWindowRemainsRunning()
{
    DockApplicationStateProjector projector;
    const auto projection = projector.project(
        snapshot({window(QStringLiteral("42"), QStringLiteral("unresolved"), false, true)}), catalog());

    const auto state = projection.states.value(QStringLiteral("app:unresolved"));
    QVERIFY(state.running);
    QVERIFY(!state.active);
}

void DockApplicationStateProjectorTest::matchedLauncherUsesDesktopTaskKey()
{
    DockApplicationStateProjector projector;
    const auto projection = projector.project(
        snapshot({window(QStringLiteral("1"), QStringLiteral("one"))}), catalog());

    QVERIFY(projection.states.contains(QStringLiteral("desktop:one.desktop")));
    QCOMPARE(projection.states.value(QStringLiteral("desktop:one.desktop")).desktopFileName,
             QStringLiteral("one.desktop"));
}

void DockApplicationStateProjectorTest::aWindowContributesToOnlyOneGroup()
{
    auto entries = catalog();
    DesktopEntryRecord duplicate;
    duplicate.desktopFileName = QStringLiteral("duplicate.desktop");
    duplicate.id = QStringLiteral("one");
    entries->byDesktopFileName.insert(duplicate.desktopFileName, entries->entries.size());
    entries->byDesktopId.insert(duplicate.id, entries->entries.size());
    entries->entries.append(duplicate);

    DockApplicationStateProjector projector;
    const auto projection = projector.project(
        snapshot({window(QStringLiteral("1"), QStringLiteral("one"))}), entries);
    QCOMPARE(projection.states.size(), 1);
    QCOMPARE(projection.states.constBegin().value().windowCount, 1);
}

void DockApplicationStateProjectorTest::encounterOrderIsUniqueAndDeterministic()
{
    DockApplicationStateProjector projector;
    const auto projection = projector.project(
        snapshot({window(QStringLiteral("1"), QStringLiteral("two")),
                  window(QStringLiteral("2"), QStringLiteral("one")),
                  window(QStringLiteral("3"), QStringLiteral("two"))}), catalog());

    const QStringList expected{QStringLiteral("desktop:two.desktop"), QStringLiteral("desktop:one.desktop")};
    QCOMPARE(projection.encounterOrder, expected);
}

void DockApplicationStateProjectorTest::focusSerialOrdersExactActivationTargets()
{
    DockApplicationStateProjector projector;
    const auto projection = projector.project(
        snapshot({window(QStringLiteral("old"), QStringLiteral("one"), false, false, 1, 4),
                  window(QStringLiteral("new"), QStringLiteral("one"), false, false, 1, 9)}),
        catalog());

    const QVector<QString> expected{QStringLiteral("new"), QStringLiteral("old")};
    QCOMPARE(projection.states.value(QStringLiteral("desktop:one.desktop")).windowIds, expected);
}

void DockApplicationStateProjectorTest::countsAreClampedToInt()
{
    Snapshot input;
    input.total = std::numeric_limits<quint32>::max();
    DockApplicationStateProjector projector;
    const auto projection = projector.project(input, catalog());
    QVERIFY(projection.states.isEmpty());
}

void DockApplicationStateProjectorTest::stress100ProjectionCycles()
{
    DockApplicationStateProjector projector;
    for (int cycle = 0; cycle < 100; ++cycle) {
        const QString id = QString::number(cycle);
        const auto projection = projector.project(
            snapshot({window(id, QStringLiteral("one"), cycle % 2 == 0, cycle % 3 == 0, 99)}),
            catalog());
        QCOMPARE(projection.states.value(QStringLiteral("desktop:one.desktop")).windowCount, 1);
        QCOMPARE(projection.states.value(QStringLiteral("desktop:one.desktop")).windowIds,
                 QVector<QString>{id});
        QVERIFY(!projection.states.contains(QStringLiteral("desktop:two.desktop")));
    }
}

void DockApplicationStateProjectorTest::catalogGainKeepsLiveTaskKey()
{
    DockApplicationStateProjector projector;
    RuntimeTaskIdentityTracker tracker;
    const auto empty = std::make_shared<DesktopEntrySnapshot>();
    const auto firstSnapshot = snapshot(
        1, {window(QStringLiteral("late-window"), QStringLiteral("late-app"))});
    const auto first = projector.project(firstSnapshot, empty,
                                         tracker.update(firstSnapshot, empty));
    const auto secondSnapshot = snapshot(
        1, {window(QStringLiteral("late-window"), QStringLiteral("late-app"))});
    const auto afterCatalog = projector.project(secondSnapshot, lateCatalog(),
                                                tracker.update(secondSnapshot, lateCatalog()));

    QCOMPARE(first.states.keys(), QStringList{QStringLiteral("app:late-app")});
    QCOMPARE(afterCatalog.states.keys(), QStringList{QStringLiteral("app:late-app")});
    QCOMPARE(afterCatalog.states.value(QStringLiteral("app:late-app")).desktopFileName,
             QStringLiteral("late.desktop"));
}

void DockApplicationStateProjectorTest::catalogLossKeepsLiveTaskKey()
{
    DockApplicationStateProjector projector;
    RuntimeTaskIdentityTracker tracker;
    const auto matched = lateCatalog();
    const auto firstSnapshot = snapshot(
        1, {window(QStringLiteral("late-window"), QStringLiteral("late-app"))});
    const auto first = projector.project(firstSnapshot, matched,
                                         tracker.update(firstSnapshot, matched));
    const auto secondSnapshot = snapshot(
        1, {window(QStringLiteral("late-window"), QStringLiteral("late-app"))});
    const auto afterCatalog = projector.project(secondSnapshot,
                                                std::make_shared<DesktopEntrySnapshot>(),
                                                tracker.update(secondSnapshot,
                                                               std::make_shared<DesktopEntrySnapshot>()));

    QCOMPARE(first.states.keys(), QStringList{QStringLiteral("desktop:late.desktop")});
    QCOMPARE(afterCatalog.states.keys(), QStringList{QStringLiteral("desktop:late.desktop")});
}

void DockApplicationStateProjectorTest::metadataChangeKeepsLiveTaskKey()
{
    DockApplicationStateProjector projector;
    RuntimeTaskIdentityTracker tracker;
    const auto empty = std::make_shared<DesktopEntrySnapshot>();
    const auto firstSnapshot = snapshot(
        1, {window(QStringLiteral("stable-window"), QStringLiteral("before-app"))});
    projector.project(firstSnapshot, empty, tracker.update(firstSnapshot, empty));

    const auto changedSnapshot = snapshot(
        1, {window(QStringLiteral("stable-window"), QStringLiteral("after-app"))});
    const auto changed = projector.project(changedSnapshot, empty,
                                           tracker.update(changedSnapshot, empty));

    QCOMPARE(changed.states.keys(), QStringList{QStringLiteral("app:before-app")});
}

void DockApplicationStateProjectorTest::metadataChangeAliasesNewWindowIntoExistingTask()
{
    DockApplicationStateProjector projector;
    RuntimeTaskIdentityTracker tracker;
    const auto empty = std::make_shared<DesktopEntrySnapshot>();
    const auto firstSnapshot = snapshot(
        1, {window(QStringLiteral("first"), QStringLiteral("before-app"))});
    const auto firstAssignments = tracker.update(firstSnapshot, empty);
    QCOMPARE(firstAssignments.value(QStringLiteral("first")), QStringLiteral("app:before-app"));

    const auto changedSnapshot = snapshot(
        1, {window(QStringLiteral("first"), QStringLiteral("after-app"))});
    const auto changedAssignments = tracker.update(changedSnapshot, empty);
    QCOMPARE(changedAssignments.value(QStringLiteral("first")), QStringLiteral("app:before-app"));

    const auto twoWindowsSnapshot = snapshot(
        1,
        {window(QStringLiteral("first"), QStringLiteral("after-app")),
         window(QStringLiteral("second"), QStringLiteral("after-app"))});
    const auto twoWindows = projector.project(
        twoWindowsSnapshot, empty, tracker.update(twoWindowsSnapshot, empty));
    QCOMPARE(twoWindows.states.size(), 1);
    const QString taskKey = QStringLiteral("app:before-app");
    QVERIFY(twoWindows.states.contains(taskKey));
    const QVector<QString> expectedWindowIds{QStringLiteral("first"), QStringLiteral("second")};
    QCOMPARE(twoWindows.states.value(taskKey).windowIds, expectedWindowIds);
    QVERIFY(!twoWindows.states.contains(QStringLiteral("app:after-app")));

    const auto endedSnapshot = snapshot(1, {});
    tracker.update(endedSnapshot, empty);
    const auto nextLifetimeSnapshot = snapshot(
        1, {window(QStringLiteral("third"), QStringLiteral("after-app"))});
    const auto nextLifetimeAssignments = tracker.update(nextLifetimeSnapshot, empty);
    QCOMPARE(nextLifetimeAssignments.value(QStringLiteral("third")), QStringLiteral("app:after-app"));
}

void DockApplicationStateProjectorTest::metadataChangeAliasIsIndependentOfWindowOrder()
{
    DockApplicationStateProjector projector;
    const auto empty = std::make_shared<DesktopEntrySnapshot>();
    const QString taskKey = QStringLiteral("app:before-app");
    const auto initialSnapshot = snapshot(
        1, {window(QStringLiteral("first"), QStringLiteral("before-app"))});

    RuntimeTaskIdentityTracker existingFirstTracker;
    existingFirstTracker.update(initialSnapshot, empty);
    const auto existingFirstSnapshot = snapshot(
        1,
        {window(QStringLiteral("first"), QStringLiteral("after-app"), false, false, 1, 10),
         window(QStringLiteral("second"), QStringLiteral("after-app"), false, false, 1, 20)});
    const auto existingFirstAssignments =
        existingFirstTracker.update(existingFirstSnapshot, empty);
    QCOMPARE(existingFirstAssignments.value(QStringLiteral("first")), taskKey);
    QCOMPARE(existingFirstAssignments.value(QStringLiteral("second")), taskKey);
    const auto existingFirstProjection =
        projector.project(existingFirstSnapshot, empty, existingFirstAssignments);
    QCOMPARE(existingFirstProjection.states.size(), 1);
    QVERIFY(existingFirstProjection.states.contains(taskKey));
    QVERIFY(!existingFirstProjection.states.contains(QStringLiteral("app:after-app")));
    QVERIFY(existingFirstProjection.states.value(taskKey).windowIds.contains(QStringLiteral("first")));
    QVERIFY(existingFirstProjection.states.value(taskKey).windowIds.contains(QStringLiteral("second")));

    RuntimeTaskIdentityTracker newFirstTracker;
    newFirstTracker.update(initialSnapshot, empty);
    const auto newFirstSnapshot = snapshot(
        1,
        {window(QStringLiteral("second"), QStringLiteral("after-app"), false, false, 1, 20),
         window(QStringLiteral("first"), QStringLiteral("after-app"), false, false, 1, 10)});
    const auto newFirstAssignments = newFirstTracker.update(newFirstSnapshot, empty);
    QCOMPARE(newFirstAssignments.value(QStringLiteral("first")), taskKey);
    QCOMPARE(newFirstAssignments.value(QStringLiteral("second")), taskKey);
    const auto newFirstProjection = projector.project(newFirstSnapshot, empty, newFirstAssignments);
    QCOMPARE(newFirstProjection.states.size(), 1);
    QVERIFY(newFirstProjection.states.contains(taskKey));
    QVERIFY(!newFirstProjection.states.contains(QStringLiteral("app:after-app")));
    QVERIFY(newFirstProjection.states.value(taskKey).windowIds.contains(QStringLiteral("first")));
    QVERIFY(newFirstProjection.states.value(taskKey).windowIds.contains(QStringLiteral("second")));
}

void DockApplicationStateProjectorTest::newWindowJoinsExistingAppCohort()
{
    DockApplicationStateProjector projector;
    RuntimeTaskIdentityTracker tracker;
    const auto empty = std::make_shared<DesktopEntrySnapshot>();
    const auto firstSnapshot = snapshot(
        1, {window(QStringLiteral("first"), QStringLiteral("late-app"))});
    projector.project(firstSnapshot, empty, tracker.update(firstSnapshot, empty));

    const auto changedSnapshot = snapshot(
        1, {window(QStringLiteral("first"), QStringLiteral("late-app")),
            window(QStringLiteral("second"), QStringLiteral("late-app"))});
    const auto changed = projector.project(changedSnapshot, lateCatalog(),
                                           tracker.update(changedSnapshot, lateCatalog()));

    const QString key = QStringLiteral("app:late-app");
    QCOMPARE(changed.states.size(), 1);
    const QVector<QString> expectedWindowIds{QStringLiteral("first"), QStringLiteral("second")};
    QCOMPARE(changed.states.value(key).windowIds, expectedWindowIds);
    QCOMPARE(changed.states.value(key).desktopFileName, QStringLiteral("late.desktop"));
}

void DockApplicationStateProjectorTest::generationChangeStartsFreshTaskLifetime()
{
    DockApplicationStateProjector projector;
    RuntimeTaskIdentityTracker tracker;
    const auto matched = lateCatalog();
    const auto firstSnapshot = snapshot(
        1, {window(QStringLiteral("reused"), QStringLiteral("late-app"))});
    const auto first = projector.project(firstSnapshot, matched,
                                         tracker.update(firstSnapshot, matched));
    const auto secondSnapshot = snapshot(
        2, {window(QStringLiteral("reused"), QStringLiteral("late-app"))});
    const auto second = projector.project(secondSnapshot,
                                          std::make_shared<DesktopEntrySnapshot>(),
                                          tracker.update(secondSnapshot,
                                                         std::make_shared<DesktopEntrySnapshot>()));

    QCOMPARE(first.states.keys(), QStringList{QStringLiteral("desktop:late.desktop")});
    QCOMPARE(second.states.keys(), QStringList{QStringLiteral("app:late-app")});
}

void DockApplicationStateProjectorTest::authorityResetStartsFreshTaskLifetime()
{
    DockApplicationStateProjector projector;
    RuntimeTaskIdentityTracker tracker;
    const auto matched = lateCatalog();
    const auto firstSnapshot = snapshot(
        1, {window(QStringLiteral("reused"), QStringLiteral("late-app"))});
    const auto first = projector.project(firstSnapshot, matched,
                                         tracker.update(firstSnapshot, matched));
    tracker.reset();
    const auto secondSnapshot = snapshot(
        1, {window(QStringLiteral("reused"), QStringLiteral("late-app"))});
    const auto second = projector.project(secondSnapshot,
                                          std::make_shared<DesktopEntrySnapshot>(),
                                          tracker.update(secondSnapshot,
                                                         std::make_shared<DesktopEntrySnapshot>()));

    QCOMPARE(first.states.keys(), QStringList{QStringLiteral("desktop:late.desktop")});
    QCOMPARE(second.states.keys(), QStringList{QStringLiteral("app:late-app")});
}

void DockApplicationStateProjectorTest::emptyAppIdWindowsRemainSeparate()
{
    DockApplicationStateProjector projector;
    RuntimeTaskIdentityTracker tracker;
    const auto empty = std::make_shared<DesktopEntrySnapshot>();
    const auto input = snapshot(
        1, {window(QStringLiteral("one"), QString()), window(QStringLiteral("two"), QString())});
    const auto projection = projector.project(input, empty, tracker.update(input, empty));

    QCOMPARE(projection.states.size(), 2);
    QVERIFY(projection.states.contains(QStringLiteral("window:one")));
    QVERIFY(projection.states.contains(QStringLiteral("window:two")));
}

QTEST_MAIN(DockApplicationStateProjectorTest)
#include "DockApplicationStateProjectorTest.moc"
