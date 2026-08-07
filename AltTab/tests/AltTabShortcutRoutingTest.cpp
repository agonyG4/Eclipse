#include <QTest>

#include "core/AltTabShortcutRouter.hpp"

class AltTabShortcutRoutingTest final : public QObject {
    Q_OBJECT

private slots:
    void mapsNativeLifecycleEvents();
    void ignoresOtherNamespacesAndTerminalPhases();
};

void AltTabShortcutRoutingTest::mapsNativeLifecycleEvents()
{
    QCOMPARE(mapTyphonShortcut(QStringLiteral("astrea-shell"), QStringLiteral("alt_tab_next"),
                               TyphonShortcutPhase::Pressed),
             AltTabShortcutAction::Next);
    QCOMPARE(mapTyphonShortcut(QStringLiteral("astrea-shell"), QStringLiteral("alt_tab_next"),
                               TyphonShortcutPhase::Repeated),
             AltTabShortcutAction::Next);
    QCOMPARE(mapTyphonShortcut(QStringLiteral("astrea-shell"), QStringLiteral("alt_tab_previous"),
                               TyphonShortcutPhase::Pressed),
             AltTabShortcutAction::Previous);
    QCOMPARE(mapTyphonShortcut(QStringLiteral("astrea-shell"), QStringLiteral("alt_tab_commit"),
                               TyphonShortcutPhase::Pressed),
             AltTabShortcutAction::Commit);
}

void AltTabShortcutRoutingTest::ignoresOtherNamespacesAndTerminalPhases()
{
    for (const auto phase : {TyphonShortcutPhase::Released, TyphonShortcutPhase::Cancelled}) {
        QCOMPARE(mapTyphonShortcut(QStringLiteral("astrea-shell"), QStringLiteral("alt_tab_next"),
                                   phase),
                 AltTabShortcutAction::Ignore);
        QCOMPARE(mapTyphonShortcut(QStringLiteral("other"), QStringLiteral("alt_tab_next"), phase),
                 AltTabShortcutAction::Ignore);
    }
    QCOMPARE(mapTyphonShortcut(QStringLiteral("other"), QStringLiteral("alt_tab_commit"),
                               TyphonShortcutPhase::Pressed),
             AltTabShortcutAction::Ignore);
    QCOMPARE(mapTyphonShortcut(QStringLiteral("astrea-shell"), QStringLiteral("unknown"),
                               TyphonShortcutPhase::Pressed),
             AltTabShortcutAction::Ignore);
}

QTEST_GUILESS_MAIN(AltTabShortcutRoutingTest)
#include "AltTabShortcutRoutingTest.moc"
