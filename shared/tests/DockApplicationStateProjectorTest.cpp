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
                bool minimized = false, quint32 pid = 1)
{
    Toplevel result;
    result.id = id;
    result.appId = appId;
    result.title = id;
    result.pid = pid;
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
    void runningAndActiveStateIsProjected();
    void minimizedWindowsRemainRunning();
    void duplicatePidsRemainSeparateAndOrdered();
    void unresolvedWindowsAreIgnored();
    void aWindowContributesToOnlyOneGroup();
    void countsAreClampedToInt();
    void stress100ProjectionCycles();
};

void DockApplicationStateProjectorTest::zeroWindowsProducesClearedPinnedState()
{
    DockApplicationStateProjector projector;
    const auto states = projector.project(snapshot({}), catalog(), {QStringLiteral("one.desktop")});
    QCOMPARE(states.value(QStringLiteral("one.desktop")).windowCount, 0);
    QVERIFY(!states.value(QStringLiteral("one.desktop")).running);
    QVERIFY(!states.value(QStringLiteral("one.desktop")).active);
}

void DockApplicationStateProjectorTest::runningAndActiveStateIsProjected()
{
    DockApplicationStateProjector projector;
    const auto states = projector.project(
        snapshot({window(QStringLiteral("1"), QStringLiteral("one"), true)}),
        catalog(), {QStringLiteral("one.desktop")});
    const auto state = states.value(QStringLiteral("one.desktop"));
    QVERIFY(state.running);
    QVERIFY(state.active);
    QCOMPARE(state.windowCount, 1);
    QCOMPARE(state.windowIds, QVector<QString>{QStringLiteral("1")});
}

void DockApplicationStateProjectorTest::minimizedWindowsRemainRunning()
{
    DockApplicationStateProjector projector;
    const auto states = projector.project(
        snapshot({window(QStringLiteral("1"), QStringLiteral("one"), false, true)}),
        catalog(), {QStringLiteral("one.desktop")});
    QVERIFY(states.value(QStringLiteral("one.desktop")).running);
    QVERIFY(!states.value(QStringLiteral("one.desktop")).active);
}

void DockApplicationStateProjectorTest::duplicatePidsRemainSeparateAndOrdered()
{
    DockApplicationStateProjector projector;
    const auto states = projector.project(
        snapshot({window(QStringLiteral("2"), QStringLiteral("one"), false, false, 4),
                  window(QStringLiteral("1"), QStringLiteral("one"), true, false, 4)}),
        catalog(), {QStringLiteral("one.desktop")});
    QCOMPARE(states.value(QStringLiteral("one.desktop")).windowCount, 2);
    const QVector<QString> expected{QStringLiteral("2"), QStringLiteral("1")};
    QCOMPARE(states.value(QStringLiteral("one.desktop")).windowIds, expected);
}

void DockApplicationStateProjectorTest::unresolvedWindowsAreIgnored()
{
    DockApplicationStateProjector projector;
    const auto states = projector.project(
        snapshot({window(QStringLiteral("1"), QStringLiteral("unknown"))}),
        catalog(), {QStringLiteral("one.desktop")});
    QCOMPARE(states.value(QStringLiteral("one.desktop")).windowCount, 0);
    QCOMPARE(states.size(), 1);
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
    const auto states = projector.project(
        snapshot({window(QStringLiteral("1"), QStringLiteral("one"))}), entries,
        {QStringLiteral("one.desktop"), QStringLiteral("duplicate.desktop")});
    QCOMPARE(states.value(QStringLiteral("one.desktop")).windowCount
             + states.value(QStringLiteral("duplicate.desktop")).windowCount, 1);
}

void DockApplicationStateProjectorTest::countsAreClampedToInt()
{
    Snapshot input;
    input.total = std::numeric_limits<quint32>::max();
    DockApplicationStateProjector projector;
    const auto states = projector.project(input, catalog(), {QStringLiteral("one.desktop")});
    QCOMPARE(states.value(QStringLiteral("one.desktop")).windowCount, 0);
}

void DockApplicationStateProjectorTest::stress100ProjectionCycles()
{
    DockApplicationStateProjector projector;
    for (int cycle = 0; cycle < 100; ++cycle) {
        const QString id = QString::number(cycle);
        const auto states = projector.project(
            snapshot({window(id, QStringLiteral("one"), cycle % 2 == 0, cycle % 3 == 0, 99)}),
            catalog(), {QStringLiteral("one.desktop"), QStringLiteral("two.desktop")});
        QCOMPARE(states.value(QStringLiteral("one.desktop")).windowCount, 1);
        QCOMPARE(states.value(QStringLiteral("one.desktop")).windowIds,
                 QVector<QString>{id});
        QCOMPARE(states.value(QStringLiteral("two.desktop")).windowCount, 0);
    }
}

QTEST_MAIN(DockApplicationStateProjectorTest)
#include "DockApplicationStateProjectorTest.moc"
