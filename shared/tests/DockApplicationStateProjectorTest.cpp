#include <QTest>

#include "apps/DesktopEntryCatalog.hpp"
#include "platform/typhon/DockApplicationStateProjector.hpp"

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

QTEST_MAIN(DockApplicationStateProjectorTest)
#include "DockApplicationStateProjectorTest.moc"
