#include <QSet>
#include <QTest>

#include "platform/typhon/TyphonActionState.hpp"

using namespace Astrea::Typhon;

class TyphonActionStateTest final : public QObject {
    Q_OBJECT

private slots:
    void reservesUniqueGenerationScopedTokens();
    void rejectsInvalidTokensWithoutPendingState();
    void rejectsDuplicateAndSixtyFifthPendingAction();
    void completionReleasesCapacityAndAllowsReuse();
    void staleOrMismatchedCompletionCannotSettleAnotherAction();
    void disconnectReturnsOnlyCurrentGenerationEntries();
};

void TyphonActionStateTest::reservesUniqueGenerationScopedTokens()
{
    TyphonActionState state;
    QSet<TyphonActionToken> tokens;
    for (quint64 sequence = 0; sequence < 64; ++sequence) {
        const TyphonActionToken token = state.nextToken(7);
        QVERIFY(token.isValid());
        QVERIFY(!tokens.contains(token));
        tokens.insert(token);
        QCOMPARE(state.reserve(7, token, QString::number(sequence),
                               ToplevelAction::Activate, sequence),
                 TyphonActionAdmission::Accepted);
    }
    QCOMPARE(state.pendingCount(), qsizetype(64));
}

void TyphonActionStateTest::rejectsInvalidTokensWithoutPendingState()
{
    TyphonActionState state;
    QCOMPARE(state.reserve(7, {}, QStringLiteral("1"), ToplevelAction::Activate, 1),
             TyphonActionAdmission::InvalidToken);
    QCOMPARE(state.pendingCount(), qsizetype(0));
}

void TyphonActionStateTest::rejectsDuplicateAndSixtyFifthPendingAction()
{
    TyphonActionState state;
    const TyphonActionToken duplicate = state.nextToken(3);
    QCOMPARE(state.reserve(3, duplicate, QStringLiteral("1"), ToplevelAction::Close, 1),
             TyphonActionAdmission::Accepted);
    QCOMPARE(state.reserve(3, duplicate, QStringLiteral("2"), ToplevelAction::Close, 2),
             TyphonActionAdmission::DuplicatePending);

    for (quint64 sequence = 1; sequence < 64; ++sequence) {
        const TyphonActionToken token = state.nextToken(3);
        QCOMPARE(state.reserve(3, token, QString::number(sequence + 1),
                               ToplevelAction::Restore, sequence + 1),
                 TyphonActionAdmission::Accepted);
    }

    const TyphonActionToken overCapacity = state.nextToken(3);
    QCOMPARE(state.reserve(3, overCapacity, QStringLiteral("65"), ToplevelAction::Minimize, 65),
             TyphonActionAdmission::CapacityExceeded);
    QCOMPARE(state.pendingCount(), qsizetype(64));
}

void TyphonActionStateTest::completionReleasesCapacityAndAllowsReuse()
{
    TyphonActionState state;
    const TyphonActionToken token = state.nextToken(11);
    QCOMPARE(state.reserve(11, token, QStringLiteral("42"), ToplevelAction::Activate, 99),
             TyphonActionAdmission::Accepted);

    const auto completion = state.complete(11, token, ToplevelAction::Activate,
                                           ToplevelActionResult::Accepted);
    QVERIFY(completion.has_value());
    QCOMPARE(completion->windowId, QStringLiteral("42"));
    QCOMPARE(completion->consumerToken, quint64(99));
    QCOMPARE(state.pendingCount(), qsizetype(0));
    QVERIFY(!state.complete(11, token, ToplevelAction::Activate,
                            ToplevelActionResult::NoChange).has_value());

    QCOMPARE(state.reserve(11, token, QStringLiteral("43"), ToplevelAction::Close, 100),
             TyphonActionAdmission::Accepted);
    QCOMPARE(state.pendingCount(), qsizetype(1));
}

void TyphonActionStateTest::staleOrMismatchedCompletionCannotSettleAnotherAction()
{
    TyphonActionState state;
    const TyphonActionToken oldToken = state.nextToken(1);
    QCOMPARE(state.reserve(1, oldToken, QStringLiteral("1"), ToplevelAction::Activate, 1),
             TyphonActionAdmission::Accepted);
    state.clearGeneration(1);

    const TyphonActionToken newToken = state.nextToken(2);
    QCOMPARE(state.reserve(2, newToken, QStringLiteral("2"), ToplevelAction::Minimize, 2),
             TyphonActionAdmission::Accepted);
    QVERIFY(!state.complete(1, oldToken, ToplevelAction::Activate,
                            ToplevelActionResult::Accepted).has_value());
    QVERIFY(!state.complete(2, newToken, ToplevelAction::Restore,
                            ToplevelActionResult::Accepted).has_value());
    QCOMPARE(state.pendingCount(), qsizetype(1));
    QVERIFY(state.complete(2, newToken, ToplevelAction::Minimize,
                           ToplevelActionResult::NoChange).has_value());
}

void TyphonActionStateTest::disconnectReturnsOnlyCurrentGenerationEntries()
{
    TyphonActionState state;
    const TyphonActionToken first = state.nextToken(5);
    const TyphonActionToken second = state.nextToken(5);
    QCOMPARE(state.reserve(5, first, QStringLiteral("1"), ToplevelAction::Activate, 10),
             TyphonActionAdmission::Accepted);
    QCOMPARE(state.reserve(5, second, QStringLiteral("2"), ToplevelAction::Close, 11),
             TyphonActionAdmission::Accepted);

    const QVector<TyphonPendingAction> disconnected = state.clearGeneration(5);
    QCOMPARE(disconnected.size(), 2);
    QCOMPARE(state.pendingCount(), qsizetype(0));
    QVERIFY(!state.complete(5, first, ToplevelAction::Activate,
                            ToplevelActionResult::Accepted).has_value());
}

QTEST_GUILESS_MAIN(TyphonActionStateTest)
#include "TyphonActionStateTest.moc"
