#include "platform/linux/AdministrativeGroupPolicy.hpp"

#include <QtTest>

class AdministrativeGroupPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void recognizesWheelMembership();
    void recognizesSudoMembership();
    void ignoresUnrelatedGroups();
    void rejectsEmptyGroupList();
};

void AdministrativeGroupPolicyTest::recognizesWheelMembership()
{
    QVERIFY(AdministrativeGroupPolicy::hasAdministrativeGroup({QStringLiteral("audio"), QStringLiteral("wheel")}));
}

void AdministrativeGroupPolicyTest::recognizesSudoMembership()
{
    QVERIFY(AdministrativeGroupPolicy::hasAdministrativeGroup({QStringLiteral("users"), QStringLiteral("sudo")}));
}

void AdministrativeGroupPolicyTest::ignoresUnrelatedGroups()
{
    QVERIFY(!AdministrativeGroupPolicy::hasAdministrativeGroup({QStringLiteral("audio"), QStringLiteral("video")}));
}

void AdministrativeGroupPolicyTest::rejectsEmptyGroupList()
{
    QVERIFY(!AdministrativeGroupPolicy::hasAdministrativeGroup({}));
}

QTEST_GUILESS_MAIN(AdministrativeGroupPolicyTest)
#include "AdministrativeGroupPolicyTest.moc"
