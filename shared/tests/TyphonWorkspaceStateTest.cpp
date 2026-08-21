#include <QTest>

#include "platform/typhon/TyphonWorkspaceState.hpp"

class TyphonWorkspaceStateTest final : public QObject {
    Q_OBJECT

private slots:
    void managerDoneCommitsBufferedWorkspaceState();
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
