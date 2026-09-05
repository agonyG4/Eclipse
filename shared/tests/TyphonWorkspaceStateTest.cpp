#include <QTest>

#include "platform/typhon/TyphonWorkspaceState.hpp"

class TyphonWorkspaceStateTest final : public QObject {
    Q_OBJECT

private slots:
    void managerDoneCommitsBufferedWorkspaceState();
    void hiddenWorkspaceReappearsWithStableIdentity();
    void stableProtocolIdIsTheIdentity();
};

void TyphonWorkspaceStateTest::managerDoneCommitsBufferedWorkspaceState()
{
    TyphonWorkspaceState state;
    state.beginGeneration(7);
    state.beginWorkspace(QStringLiteral("typhon.workspace.4"));
    state.setWorkspaceName(QStringLiteral("4"));
    state.setWorkspaceState(1u);
    QVERIFY(state.committedWorkspaces().isEmpty());

    state.commitDone(7);
    QCOMPARE(state.committedWorkspaces().size(), 1);
    QVERIFY(state.committedWorkspaces().front().active);
    QVERIFY(!state.committedWorkspaces().front().hidden);
}

void TyphonWorkspaceStateTest::hiddenWorkspaceReappearsWithStableIdentity()
{
    TyphonWorkspaceState state;
    state.beginGeneration(8);
    state.beginWorkspace(QStringLiteral("typhon.workspace.4"));
    state.setWorkspaceName(QStringLiteral("4"));
    state.setWorkspaceState(4u);
    QVERIFY(state.commitDone(8));
    QVERIFY(state.committedWorkspaces().isEmpty());

    state.beginWorkspace(QStringLiteral("typhon.workspace.4"));
    state.setWorkspaceState(0u);
    QVERIFY(state.commitDone(8));
    QCOMPARE(state.committedWorkspaces().size(), 1);
    QCOMPARE(state.committedWorkspaces().front().id,
             QStringLiteral("typhon.workspace.4"));
    QCOMPARE(state.committedWorkspaces().front().name, QStringLiteral("4"));
    QVERIFY(!state.committedWorkspaces().front().active);
    QVERIFY(!state.committedWorkspaces().front().hidden);
}

void TyphonWorkspaceStateTest::stableProtocolIdIsTheIdentity()
{
    TyphonWorkspaceState state;
    state.beginGeneration(1);
    state.beginWorkspace(QStringLiteral("typhon.workspace.4"));
    state.setWorkspaceName(QStringLiteral("renamed"));

    QCOMPARE(state.pendingWorkspaces().front().id,
             QStringLiteral("typhon.workspace.4"));
}

QTEST_MAIN(TyphonWorkspaceStateTest)
#include "TyphonWorkspaceStateTest.moc"
