#include <QTest>

#include "platform/typhon/AltTabShortcutAdapter.hpp"

class AltTabShortcutRoutingTest final : public QObject {
    Q_OBJECT

private slots:
    void mapsNativeLifecycleEvents();
    void mapsPlatformNeutralLifecyclePhases();
    void ignoresNonAltTabShellFeatures();
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

void AltTabShortcutRoutingTest::mapsPlatformNeutralLifecyclePhases()
{
    QCOMPARE(mapAltTabShortcut(QStringLiteral("astrea-shell"), QStringLiteral("alt_tab_next"),
                               AltTabShortcutPhase::Pressed),
             AltTabShortcutAction::Next);
    QCOMPARE(mapAltTabShortcut(QStringLiteral("astrea-shell"), QStringLiteral("alt_tab_next"),
                               AltTabShortcutPhase::Repeated),
             AltTabShortcutAction::Next);
    QCOMPARE(mapAltTabShortcut(QStringLiteral("astrea-shell"), QStringLiteral("alt_tab_commit"),
                               AltTabShortcutPhase::Released),
             AltTabShortcutAction::Ignore);
}

void AltTabShortcutRoutingTest::ignoresNonAltTabShellFeatures()
{
    QCOMPARE(mapTyphonShortcut(QStringLiteral("astrea-shell"), QStringLiteral("spotlight_toggle"),
                               TyphonShortcutPhase::Pressed),
             AltTabShortcutAction::Ignore);
    for (const QString &name : {QStringLiteral("screenshot_quick"),
                                QStringLiteral("screenshot_region_frozen"),
                                QStringLiteral("screenshot_region_live")}) {
        QCOMPARE(mapTyphonShortcut(QStringLiteral("astrea-shell"), name,
                                   TyphonShortcutPhase::Pressed),
                 AltTabShortcutAction::Ignore);
    }
    QCOMPARE(mapTyphonShortcut(QStringLiteral("astrea-shell"), QStringLiteral("spotlight_toggle"),
                               TyphonShortcutPhase::Repeated),
             AltTabShortcutAction::Ignore);
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
