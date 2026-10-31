#include "services/network/SettingsFirewallStatus.hpp"
#include <QtTest>

class SettingsFirewallStatusTest final : public QObject {
    Q_OBJECT
private slots:
    void classifiesAuthoritativeUnitStates()
    {
        QCOMPARE(SettingsFirewallStatus::classifyUnitState(QStringLiteral("active")),
                 SettingsFirewallStatus::Active);
        QCOMPARE(SettingsFirewallStatus::classifyUnitState(QStringLiteral("inactive")),
                 SettingsFirewallStatus::Inactive);
        QCOMPARE(SettingsFirewallStatus::classifyUnitState(QStringLiteral("failed")),
                 SettingsFirewallStatus::Inactive);
        for (const QString &value : {QString(), QStringLiteral("activating"),
                                     QStringLiteral("deactivating"), QStringLiteral("unknown")}) {
            QCOMPARE(SettingsFirewallStatus::classifyUnitState(value),
                     SettingsFirewallStatus::Unknown);
        }
    }
    void exposesReadOnlyProperties()
    {
        SettingsFirewallStatus provider;
        QVERIFY(!provider.active() || provider.known());
        QVERIFY(provider.metaObject()->indexOfMethod("refresh()") >= 0);
        QVERIFY(provider.metaObject()->indexOfProperty("state") >= 0);
    }
};

QTEST_GUILESS_MAIN(SettingsFirewallStatusTest)
#include "SettingsFirewallStatusTest.moc"
