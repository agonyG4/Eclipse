#include "platform/linux/AdminGroupDetector.hpp"
#include "services/profile/SettingsUserProfileProvider.hpp"

#include <QtTest>

class SettingsUserProfileProviderTest final : public QObject {
    Q_OBJECT

private slots:
    void resolvesCurrentUserAndAdministratorState();
};

void SettingsUserProfileProviderTest::resolvesCurrentUserAndAdministratorState()
{
    const QByteArray previousUser = qgetenv("USER");
    const QByteArray previousLogName = qgetenv("LOGNAME");
    qputenv("USER", QByteArrayLiteral("profile-test-user"));
    qunsetenv("LOGNAME");

    AdminGroupDetector::NativeApi api;
    api.currentUser = [] {
        return std::optional<AdminGroupUser>{AdminGroupUser{QByteArrayLiteral("profile-test-user"), 1000}};
    };
    api.getGroupList = [](const QByteArray &, gid_t, gid_t *groups, int *) {
        groups[0] = 1001;
        return 1;
    };
    api.groupName = [](gid_t) { return std::optional<QString>{QStringLiteral("wheel")}; };

    const SettingsUserProfile profile = SettingsUserProfileProvider(AdminGroupDetector(api)).currentProfile();

    if (previousUser.isEmpty())
        qunsetenv("USER");
    else
        qputenv("USER", previousUser);
    if (previousLogName.isEmpty())
        qunsetenv("LOGNAME");
    else
        qputenv("LOGNAME", previousLogName);

    QCOMPARE(profile.userName, QStringLiteral("profile-test-user"));
    QVERIFY(profile.avatarUrl.isEmpty());
    QVERIFY(profile.administrator);
}

QTEST_GUILESS_MAIN(SettingsUserProfileProviderTest)
#include "SettingsUserProfileProviderTest.moc"
