#include <QSignalSpy>
#include <QTest>

#include "platform/typhon/TyphonToplevelModel.hpp"

using namespace Astrea::Typhon;

namespace {

void addComplete(TyphonToplevelModel &model, quint64 generation, quint64 token,
                 const QString &id, Revision revision, FocusSerial focusSerial = 0,
                 bool active = false)
{
    QVERIFY(model.handleCreated(generation, token) == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.identifierChanged(generation, token, id) == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.appIdChanged(generation, token, QStringLiteral("org.example.App"))
            == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.titleChanged(generation, token, QStringLiteral("Window ") + id)
            == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.pidChanged(generation, token, 1000 + static_cast<quint32>(token))
            == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.kindChanged(generation, token, ToplevelKind::XdgToplevel)
            == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.stateChanged(generation, token,
                               active ? ToplevelStates(ToplevelStateFlag::Active) : ToplevelStates{})
            == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.focusSerialChanged(generation, token, focusSerial)
            == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.handleDone(generation, token, revision)
            == TyphonToplevelModel::EventResult::Accepted);
}

void commit(TyphonToplevelModel &model, quint64 generation, Revision revision,
            quint32 total, bool truncated = false)
{
    QVERIFY(model.managerDone(generation, revision, total, truncated)
            == TyphonToplevelModel::EventResult::Accepted);
}

} // namespace

class TyphonToplevelModelTest final : public QObject {
    Q_OBJECT

private slots:
    void zeroWindowInitialRevisionCommits();
    void fieldsMayArriveAcrossDispatchTurns();
    void managerDoneIsTheOnlyPublicationBoundary();
    void updatesCommitAtomically();
    void closeRemovesAtCommitAndRemapGetsNewToken();
    void focusAndNumericIdOrderingAreDeterministic();
    void totalAndTruncationArePreserved();
    void duplicateIdentifierIsRejected();
    void identifierMutationIsRejected();
    void eventsAfterClosedAreRejected();
    void mismatchedHandleRevisionIsRejected();
    void incompleteHandleAtManagerDoneIsRejected();
    void staleGenerationIsIgnored();
    void managerFailureClearsPendingState();
};

void TyphonToplevelModelTest::zeroWindowInitialRevisionCommits()
{
    TyphonToplevelModel model;
    model.startGeneration(4);
    QSignalSpy snapshotSpy(&model, &TyphonToplevelModel::snapshotCommitted);

    commit(model, 4, 1, 0);

    QCOMPARE(snapshotSpy.count(), 1);
    const Snapshot snapshot = snapshotSpy.at(0).at(0).value<Snapshot>();
    QVERIFY(snapshot.windows.isEmpty());
    QCOMPARE(snapshot.revision, quint64(1));
    QCOMPARE(snapshot.total, quint32(0));
    QCOMPARE(snapshot.connectionGeneration, quint64(4));
    QVERIFY(model.hasCommittedSnapshot());
}

void TyphonToplevelModelTest::fieldsMayArriveAcrossDispatchTurns()
{
    TyphonToplevelModel model;
    model.startGeneration(1);
    QVERIFY(model.handleCreated(1, 7) == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.identifierChanged(1, 7, QStringLiteral("18446744073709551615"))
            == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(!model.hasCommittedSnapshot());

    QVERIFY(model.appIdChanged(1, 7, QStringLiteral("org.example.App"))
            == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.titleChanged(1, 7, QStringLiteral("Example"))
            == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.pidChanged(1, 7, 42) == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.kindChanged(1, 7, ToplevelKind::X11Dialog)
            == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.stateChanged(1, 7, ToplevelStates(ToplevelStateFlag::Minimized))
            == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.focusSerialChanged(1, 7, 99) == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.handleDone(1, 7, 3) == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(!model.hasCommittedSnapshot());

    commit(model, 1, 3, 1);
    QCOMPARE(model.snapshot().windows.size(), 1);
    QCOMPARE(model.snapshot().windows.first().id, QStringLiteral("18446744073709551615"));
    QCOMPARE(model.snapshot().windows.first().kind, ToplevelKind::X11Dialog);
}

void TyphonToplevelModelTest::managerDoneIsTheOnlyPublicationBoundary()
{
    TyphonToplevelModel model;
    model.startGeneration(1);
    QSignalSpy snapshotSpy(&model, &TyphonToplevelModel::snapshotCommitted);

    addComplete(model, 1, 1, QStringLiteral("10"), 1);
    QCOMPARE(snapshotSpy.count(), 0);
    commit(model, 1, 1, 1);
    QCOMPARE(snapshotSpy.count(), 1);
}

void TyphonToplevelModelTest::updatesCommitAtomically()
{
    TyphonToplevelModel model;
    model.startGeneration(1);
    addComplete(model, 1, 1, QStringLiteral("10"), 1);
    addComplete(model, 1, 2, QStringLiteral("20"), 1);
    commit(model, 1, 1, 2);

    QVERIFY(model.titleChanged(1, 1, QStringLiteral("updated"))
            == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.handleDone(1, 1, 2) == TyphonToplevelModel::EventResult::Accepted);
    QCOMPARE(model.snapshot().windows.first().title, QStringLiteral("Window 10"));
    commit(model, 1, 2, 2);
    const auto windows = model.snapshot().windows;
    QCOMPARE(windows.size(), 2);
    QCOMPARE(windows.at(0).title, QStringLiteral("updated"));
    QCOMPARE(model.lastCommittedRevision(), quint64(2));
}

void TyphonToplevelModelTest::closeRemovesAtCommitAndRemapGetsNewToken()
{
    TyphonToplevelModel model;
    model.startGeneration(1);
    addComplete(model, 1, 1, QStringLiteral("77"), 1);
    commit(model, 1, 1, 1);

    QVERIFY(model.handleClosed(1, 1) == TyphonToplevelModel::EventResult::Accepted);
    QCOMPARE(model.snapshot().windows.size(), 1);
    commit(model, 1, 2, 0);
    QVERIFY(model.snapshot().windows.isEmpty());

    addComplete(model, 1, 2, QStringLiteral("77"), 3);
    commit(model, 1, 3, 1);
    QCOMPARE(model.snapshot().windows.size(), 1);
    QCOMPARE(model.snapshot().windows.first().id, QStringLiteral("77"));
}

void TyphonToplevelModelTest::focusAndNumericIdOrderingAreDeterministic()
{
    TyphonToplevelModel model;
    model.startGeneration(1);
    addComplete(model, 1, 1, QStringLiteral("10"), 1, 5);
    addComplete(model, 1, 2, QStringLiteral("2"), 1, 5, true);
    addComplete(model, 1, 3, QStringLiteral("1"), 1, 0);
    commit(model, 1, 1, 3);

    const auto windows = model.snapshot().windows;
    QCOMPARE(windows.at(0).id, QStringLiteral("2"));
    QCOMPARE(windows.at(1).id, QStringLiteral("10"));
    QCOMPARE(windows.at(2).id, QStringLiteral("1"));
}

void TyphonToplevelModelTest::totalAndTruncationArePreserved()
{
    TyphonToplevelModel model;
    model.startGeneration(1);
    addComplete(model, 1, 1, QStringLiteral("1"), 1);
    commit(model, 1, 1, 257, true);

    QCOMPARE(model.snapshot().total, quint32(257));
    QVERIFY(model.snapshot().truncated);
}

void TyphonToplevelModelTest::duplicateIdentifierIsRejected()
{
    TyphonToplevelModel model;
    model.startGeneration(1);
    addComplete(model, 1, 1, QStringLiteral("1"), 1);
    addComplete(model, 1, 2, QStringLiteral("2"), 1);
    commit(model, 1, 1, 2);

    QVERIFY(model.identifierChanged(1, 2, QStringLiteral("1"))
            == TyphonToplevelModel::EventResult::Rejected);
    QVERIFY(model.isDegraded());
}

void TyphonToplevelModelTest::identifierMutationIsRejected()
{
    TyphonToplevelModel model;
    model.startGeneration(1);
    addComplete(model, 1, 1, QStringLiteral("1"), 1);
    commit(model, 1, 1, 1);

    QVERIFY(model.identifierChanged(1, 1, QStringLiteral("2"))
            == TyphonToplevelModel::EventResult::Rejected);
    QVERIFY(model.isDegraded());
}

void TyphonToplevelModelTest::eventsAfterClosedAreRejected()
{
    TyphonToplevelModel model;
    model.startGeneration(1);
    addComplete(model, 1, 1, QStringLiteral("1"), 1);
    commit(model, 1, 1, 1);

    QVERIFY(model.handleClosed(1, 1) == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.titleChanged(1, 1, QStringLiteral("late"))
            == TyphonToplevelModel::EventResult::Rejected);
    QVERIFY(model.isDegraded());
}

void TyphonToplevelModelTest::mismatchedHandleRevisionIsRejected()
{
    TyphonToplevelModel model;
    model.startGeneration(1);
    addComplete(model, 1, 1, QStringLiteral("1"), 2);

    QVERIFY(model.managerDone(1, 1, 1, false) == TyphonToplevelModel::EventResult::Rejected);
    QVERIFY(model.isDegraded());
}

void TyphonToplevelModelTest::incompleteHandleAtManagerDoneIsRejected()
{
    TyphonToplevelModel model;
    model.startGeneration(1);
    QVERIFY(model.handleCreated(1, 1) == TyphonToplevelModel::EventResult::Accepted);
    QVERIFY(model.identifierChanged(1, 1, QStringLiteral("1"))
            == TyphonToplevelModel::EventResult::Accepted);

    QVERIFY(model.managerDone(1, 1, 1, false) == TyphonToplevelModel::EventResult::Rejected);
    QVERIFY(!model.hasCommittedSnapshot());
}

void TyphonToplevelModelTest::staleGenerationIsIgnored()
{
    TyphonToplevelModel model;
    model.startGeneration(2);
    QVERIFY(model.handleCreated(1, 1) == TyphonToplevelModel::EventResult::IgnoredStaleGeneration);
    QVERIFY(!model.isDegraded());
    QVERIFY(model.handleCreated(2, 1) == TyphonToplevelModel::EventResult::Accepted);
}

void TyphonToplevelModelTest::managerFailureClearsPendingState()
{
    TyphonToplevelModel model;
    model.startGeneration(1);
    addComplete(model, 1, 1, QStringLiteral("1"), 1);
    commit(model, 1, 1, 1);
    QVERIFY(model.handleClosed(1, 1) == TyphonToplevelModel::EventResult::Accepted);

    model.managerFailed(1);
    QVERIFY(model.isDegraded());
    QVERIFY(!model.hasCommittedSnapshot());
    QVERIFY(!model.hasPendingTransaction());
}

QTEST_MAIN(TyphonToplevelModelTest)
#include "TyphonToplevelModelTest.moc"
