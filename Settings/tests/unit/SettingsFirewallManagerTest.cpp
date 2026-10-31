#include "services/network/SettingsFirewallManager.hpp"

#include <QtTest>

class SettingsFirewallManagerTest final : public QObject {
    Q_OBJECT
private slots:
    void validatesPrivilegedArguments()
    {
        auto policy = SettingsFirewallManager::policyCommand("incoming", "deny");
        QVERIFY(policy.has_value());
        QCOMPARE(*policy, (QStringList{"default", "deny", "incoming"}));
        QVERIFY(!SettingsFirewallManager::policyCommand("forward", "allow"));
        QVERIFY(!SettingsFirewallManager::policyCommand("incoming", "allow;reboot"));
        QVERIFY(!SettingsFirewallManager::policyCommand("outgoing", "reject"));

        const auto allow = SettingsFirewallManager::ruleCommand(false, "allow", 443, "tcp", "");
        QVERIFY(allow);
        QCOMPARE(*allow, (QStringList{"allow", "443/tcp"}));
        const auto limited = SettingsFirewallManager::ruleCommand(false, "deny", 53, "udp",
                                                                    "192.168.10.0/24");
        QVERIFY(limited);
        QCOMPARE(*limited, (QStringList{"deny", "from", "192.168.10.0/24",
                                         "to", "any", "port", "53", "proto", "udp"}));
        const auto remove = SettingsFirewallManager::ruleCommand(true, "allow", 443, "tcp", "any");
        QVERIFY(remove);
        QCOMPARE(*remove, (QStringList{"--force", "delete", "allow", "443/tcp"}));

        QVERIFY(!SettingsFirewallManager::ruleCommand(false, "allow", 0, "tcp", ""));
        QVERIFY(!SettingsFirewallManager::ruleCommand(false, "allow", 65536, "tcp", ""));
        QVERIFY(!SettingsFirewallManager::ruleCommand(false, "allow", 22, "tcp;id", ""));
        QVERIFY(!SettingsFirewallManager::ruleCommand(false, "allow;disable", 22, "tcp", ""));
        QVERIFY(!SettingsFirewallManager::ruleCommand(false, "allow", 22, "udp", "example.com"));
        QVERIFY(!SettingsFirewallManager::ruleCommand(false, "deny", 22, "tcp", "1.2.3.4;sh"));
        QVERIFY(!SettingsFirewallManager::ruleCommand(false, "deny", 22, "tcp", "192.168.1.0/33"));
        QVERIFY(!SettingsFirewallManager::ruleCommand(false, "deny", 22, "tcp", "2001:db8::1/129"));
    }

    void parsesAuthoritativeUfwOutput()
    {
        const QString output = QStringLiteral(R"(Status: active
Logging: on (low)
Default: deny (incoming), allow (outgoing), disabled (routed)
New profiles: skip

To                         Action      From
--                         ------      ----
22/tcp                     ALLOW IN    Anywhere
53/udp                     DENY IN     192.0.2.0/24
22/tcp (v6)                ALLOW IN    Anywhere (v6)
443/tcp                    ALLOW OUT   Anywhere
OpenSSH                    ALLOW IN    Anywhere
)");
        const auto state = SettingsFirewallManager::parseStatus(output);
        QVERIFY(state.valid);
        QVERIFY(state.active);
        QVERIFY(state.logging);
        QCOMPARE(state.incoming, QStringLiteral("deny"));
        QCOMPARE(state.outgoing, QStringLiteral("allow"));
        QCOMPARE(state.rules.size(), 3); // Paired v4/v6 port rule deduplicated.
        const auto port = state.rules[0].toMap();
        QCOMPARE(port.value("target").toString(), QStringLiteral("22/tcp"));
        QVERIFY(port.value("removable").toBool());
        QCOMPARE(port.value("sourceSpec").toString(), QStringLiteral("any"));
        const auto cidr = state.rules[1].toMap();
        QVERIFY(cidr.value("removable").toBool());
        QCOMPARE(cidr.value("sourceSpec").toString(), QStringLiteral("192.0.2.0/24"));
        const auto appProfile = state.rules[2].toMap();
        QVERIFY(!appProfile.value("removable").toBool());
        QCOMPARE(appProfile.value("target").toString(), QStringLiteral("OpenSSH"));

        const auto inactive = SettingsFirewallManager::parseStatus("Status: inactive\n");
        QVERIFY(inactive.valid);
        QVERIFY(!inactive.active);
        QVERIFY(inactive.rules.isEmpty());
        QVERIFY(!SettingsFirewallManager::parseStatus("unexpected output\n").valid);
    }

    void neverWritesFirewallDuringTests()
    {
        SettingsFirewallManager manager;
        QVERIFY(manager.metaObject()->indexOfProperty("knownGraphicalAgentInstalled") >= 0);
        QVERIFY(!manager.loaded());
        QVERIFY(!manager.setEnabled(false));
        QVERIFY(!manager.setDefaultPolicy("incoming", "allow"));
        QVERIFY(!manager.addRule("allow", 22, "tcp", ""));
        QVERIFY(!manager.removeRule("allow", 22, "tcp", ""));
        QVERIFY(!manager.loggingEnabled());
    }
};

QTEST_GUILESS_MAIN(SettingsFirewallManagerTest)
#include "SettingsFirewallManagerTest.moc"
