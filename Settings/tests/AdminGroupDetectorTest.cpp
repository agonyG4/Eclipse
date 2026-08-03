#include "core/AdminGroupDetector.hpp"

#include <QtTest>

#include <functional>

namespace {

AdminGroupDetector::NativeApi makeApi(
    const std::function<int(const QByteArray &, gid_t, gid_t *, int *)> &getGroupList,
    const std::function<std::optional<QString>(gid_t)> &groupName)
{
    AdminGroupDetector::NativeApi api;
    api.currentUser = [] {
        return std::optional<AdminGroupUser>{AdminGroupUser{QByteArrayLiteral("test-user"), 1000}};
    };
    api.getGroupList = getGroupList;
    api.groupName = groupName;
    return api;
}

} // namespace

class AdminGroupDetectorTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptsImmediateSuccessWithNonZeroCount();
    void retriesAfterBufferTooSmall();
    void recognizesWheelMembership();
    void recognizesSudoMembership();
    void rejectsUnrelatedGroups();
    void rejectsEmptyGroups();
    void rejectsGroupNameLookupFailure();
    void stopsAfterRepeatedResizeFailures();
};

void AdminGroupDetectorTest::acceptsImmediateSuccessWithNonZeroCount()
{
    int calls = 0;
    const auto api = makeApi(
        [&calls](const QByteArray &, gid_t, gid_t *groups, int *) {
            ++calls;
            groups[0] = 1001;
            groups[1] = 1002;
            return 2;
        },
        [](gid_t groupId) -> std::optional<QString> {
            return groupId == 1001 ? std::optional<QString>{QStringLiteral("audio")}
                                   : std::optional<QString>{QStringLiteral("wheel")};
        });

    QVERIFY(AdminGroupDetector(api).isCurrentUserAdministrator());
    QCOMPARE(calls, 1);
}

void AdminGroupDetectorTest::retriesAfterBufferTooSmall()
{
    int calls = 0;
    const auto api = makeApi(
        [&calls](const QByteArray &, gid_t, gid_t *groups, int *count) {
            ++calls;
            if (calls == 1) {
                *count = 32;
                return -1;
            }
            groups[0] = 1001;
            return 1;
        },
        [](gid_t) { return std::optional<QString>{QStringLiteral("sudo")}; });

    QVERIFY(AdminGroupDetector(api).isCurrentUserAdministrator());
    QCOMPARE(calls, 2);
}

void AdminGroupDetectorTest::recognizesWheelMembership()
{
    const auto api = makeApi(
        [](const QByteArray &, gid_t, gid_t *groups, int *) {
            groups[0] = 1001;
            return 1;
        },
        [](gid_t) { return std::optional<QString>{QStringLiteral("wheel")}; });

    QVERIFY(AdminGroupDetector(api).isCurrentUserAdministrator());
}

void AdminGroupDetectorTest::recognizesSudoMembership()
{
    const auto api = makeApi(
        [](const QByteArray &, gid_t, gid_t *groups, int *) {
            groups[0] = 1001;
            return 1;
        },
        [](gid_t) { return std::optional<QString>{QStringLiteral("sudo")}; });

    QVERIFY(AdminGroupDetector(api).isCurrentUserAdministrator());
}

void AdminGroupDetectorTest::rejectsUnrelatedGroups()
{
    const auto api = makeApi(
        [](const QByteArray &, gid_t, gid_t *groups, int *) {
            groups[0] = 1001;
            return 1;
        },
        [](gid_t) { return std::optional<QString>{QStringLiteral("audio")}; });

    QVERIFY(!AdminGroupDetector(api).isCurrentUserAdministrator());
}

void AdminGroupDetectorTest::rejectsEmptyGroups()
{
    const auto api = makeApi(
        [](const QByteArray &, gid_t, gid_t *, int *) { return 0; },
        [](gid_t) -> std::optional<QString> { return std::nullopt; });

    QVERIFY(!AdminGroupDetector(api).isCurrentUserAdministrator());
}

void AdminGroupDetectorTest::rejectsGroupNameLookupFailure()
{
    const auto api = makeApi(
        [](const QByteArray &, gid_t, gid_t *groups, int *) {
            groups[0] = 1001;
            return 1;
        },
        [](gid_t) -> std::optional<QString> { return std::nullopt; });

    QVERIFY(!AdminGroupDetector(api).isCurrentUserAdministrator());
}

void AdminGroupDetectorTest::stopsAfterRepeatedResizeFailures()
{
    int calls = 0;
    const auto api = makeApi(
        [&calls](const QByteArray &, gid_t, gid_t *, int *count) {
            ++calls;
            *count += 1;
            return -1;
        },
        [](gid_t) { return std::optional<QString>{QStringLiteral("wheel")}; });

    QVERIFY(!AdminGroupDetector(api).isCurrentUserAdministrator());
    QVERIFY(calls < 32);
}

QTEST_GUILESS_MAIN(AdminGroupDetectorTest)
#include "AdminGroupDetectorTest.moc"
