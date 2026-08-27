#include "core/SettingsGroupMembership.hpp"

#include <QtTest>

class SettingsGroupMembershipTest final : public QObject {
    Q_OBJECT

private slots:
    void recognizesWheelMembership();
    void recognizesSudoMembership();
    void ignoresUnrelatedGroups();
    void rejectsEmptyGroupList();
};

void SettingsGroupMembershipTest::recognizesWheelMembership()
{
    QVERIFY(hasAdministrativeGroup({QStringLiteral("audio"), QStringLiteral("wheel")}));
}

void SettingsGroupMembershipTest::recognizesSudoMembership()
{
    QVERIFY(hasAdministrativeGroup({QStringLiteral("users"), QStringLiteral("sudo")}));
}

void SettingsGroupMembershipTest::ignoresUnrelatedGroups()
{
    QVERIFY(!hasAdministrativeGroup({QStringLiteral("audio"), QStringLiteral("video")}));
}

void SettingsGroupMembershipTest::rejectsEmptyGroupList()
{
    QVERIFY(!hasAdministrativeGroup({}));
}

QTEST_GUILESS_MAIN(SettingsGroupMembershipTest)
#include "SettingsGroupMembershipTest.moc"
