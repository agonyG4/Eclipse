#include <QTest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

#include "core/WindowInfo.hpp"
#include "core/CompositorTypes.hpp"
#include "platform/hyprland/HyprlandCommand.hpp"

class TestWindowInfoParsing : public QObject {
    Q_OBJECT

private slots:
    void testAddressNormalization();
    void testHyprlandAddressNormalization();
    void testFromJsonHidden();
    void testFromJsonInvalidWorkspace();
    void testFromJsonValidClient();
    void testDisplayNameSteamApp();
    void testDisplayNameNormalClass();
    void testDisplayNameQuickshell();
    void testNeedsDeepIconPositive();
    void testNeedsDeepIconNegative();
    void testSortOrder();
    void testStableKey();
    void testWorkspaceIdInt();

    // Wire format tests
    void testJsonInfoRequest();
    void testEvalRequest();
    void testFocusWorkspaceRequest();
    void testFocusWindowRequest();
};

void TestWindowInfoParsing::testAddressNormalization() {
    QCOMPARE(WindowInfo::normalizeAddress(QStringLiteral("1234")), QStringLiteral("0x1234"));
    QCOMPARE(WindowInfo::normalizeAddress(QStringLiteral("0x1234")), QStringLiteral("0x1234"));
    QCOMPARE(WindowInfo::normalizeAddress(QStringLiteral("0XABCD")), QStringLiteral("0xabcd"));
    QCOMPARE(WindowInfo::normalizeAddress(QString()), QString());
    QCOMPARE(WindowInfo::normalizeAddress(QStringLiteral("")), QString());
}

void TestWindowInfoParsing::testHyprlandAddressNormalization() {
    QCOMPARE(HyprlandCommand::normalizeAddress(QStringLiteral("1234")), QStringLiteral("address:0x1234"));
    QCOMPARE(HyprlandCommand::normalizeAddress(QStringLiteral("0x1234")), QStringLiteral("address:0x1234"));
    QCOMPARE(HyprlandCommand::normalizeAddress(QStringLiteral("address:1234")), QStringLiteral("address:0x1234"));
    QCOMPARE(HyprlandCommand::normalizeAddress(QStringLiteral("address:0x1234")), QStringLiteral("address:0x1234"));
    QCOMPARE(HyprlandCommand::normalizeAddress(QString()), QString());
    QCOMPARE(HyprlandCommand::normalizeAddress(QStringLiteral("0x")), QString());
    QCOMPARE(HyprlandCommand::normalizeAddress(QStringLiteral("address:")), QString());
    QCOMPARE(HyprlandCommand::normalizeAddress(QStringLiteral("address:0x")), QString());
    QCOMPARE(HyprlandCommand::normalizeAddress(QStringLiteral("not-hex")), QString());

    // Reject embedded spaces and newlines
    QCOMPARE(HyprlandCommand::normalizeAddress(QStringLiteral("12 34")), QString());
    QCOMPARE(HyprlandCommand::normalizeAddress(QStringLiteral("12\n34")), QString());

    // Reject double prefix
    QCOMPARE(HyprlandCommand::normalizeAddress(QStringLiteral("address:address:0x1234")), QString());

    // Reject non-hex text
    QCOMPARE(HyprlandCommand::normalizeAddress(QStringLiteral("lua fragment")), QString());

    // Normalize to lowercase
    QCOMPARE(HyprlandCommand::normalizeAddress(QStringLiteral("ABCDEF")), QStringLiteral("address:0xabcdef"));
    QCOMPARE(HyprlandCommand::normalizeAddress(QStringLiteral("0xABCDEF")), QStringLiteral("address:0xabcdef"));
}

void TestWindowInfoParsing::testFromJsonHidden() {
    QJsonObject obj;
    obj[QStringLiteral("hidden")] = true;
    obj[QStringLiteral("address")] = QStringLiteral("0x1234");

    WindowInfo info = WindowInfo::fromJson(obj);
    QVERIFY(info.windowId.isEmpty() || info.isHidden);
}

void TestWindowInfoParsing::testFromJsonInvalidWorkspace() {
    QJsonObject obj;
    obj[QStringLiteral("address")] = QStringLiteral("0x1234");
    QJsonObject ws;
    ws[QStringLiteral("id")] = 0;
    obj[QStringLiteral("workspace")] = ws;

    WindowInfo info = WindowInfo::fromJson(obj);
    QVERIFY(info.workspaceIdInt() <= 0 || info.windowId.isEmpty());
}

void TestWindowInfoParsing::testFromJsonValidClient() {
    QJsonObject obj;
    obj[QStringLiteral("address")] = QStringLiteral("0x1234");
    obj[QStringLiteral("pid")] = 1234;
    obj[QStringLiteral("class")] = QStringLiteral("Firefox");
    obj[QStringLiteral("title")] = QStringLiteral("Mozilla Firefox");
    obj[QStringLiteral("focusHistoryID")] = 0;
    QJsonObject ws;
    ws[QStringLiteral("id")] = 1;
    ws[QStringLiteral("name")] = QStringLiteral("1");
    obj[QStringLiteral("workspace")] = ws;

    WindowInfo info = WindowInfo::fromJson(obj);
    QCOMPARE(info.windowId.value, QStringLiteral("0x1234"));
    QCOMPARE(info.pid, 1234LL);
    QCOMPARE(info.className, QStringLiteral("Firefox"));
    QCOMPARE(info.title, QStringLiteral("Mozilla Firefox"));
    QCOMPARE(info.workspaceIdInt(), 1);
    QCOMPARE(info.focusHistoryId, 0);
}

void TestWindowInfoParsing::testDisplayNameSteamApp() {
    QCOMPARE(WindowInfo::displayNameFromMetadata(QStringLiteral("steam_app_730"), QStringLiteral("Counter-Strike 2")),
             QStringLiteral("Counter-Strike 2"));
}

void TestWindowInfoParsing::testDisplayNameNormalClass() {
    QCOMPARE(WindowInfo::displayNameFromMetadata(QStringLiteral("firefox"), QString()),
             QStringLiteral("Firefox"));
    QCOMPARE(WindowInfo::displayNameFromMetadata(QStringLiteral("org.kde.dolphin"), QString()),
             QStringLiteral("Org Kde Dolphin"));
}

void TestWindowInfoParsing::testDisplayNameQuickshell() {
    QCOMPARE(WindowInfo::displayNameFromMetadata(QStringLiteral("org.quickshell"), QStringLiteral("Astrea Settings")),
             QStringLiteral("Astrea Settings"));
}

void TestWindowInfoParsing::testNeedsDeepIconPositive() {
    WindowInfo w;
    w.className = QStringLiteral("explorer.exe");
    QVERIFY(w.needsDeepIcon());

    w = WindowInfo();
    w.className = QStringLiteral("steam_app_730");
    QVERIFY(w.needsDeepIcon());

    w = WindowInfo();
    w.className = QStringLiteral("proton");
    QVERIFY(w.needsDeepIcon());

    w = WindowInfo();
    w.title = QStringLiteral("wine");
    QVERIFY(w.needsDeepIcon());
}

void TestWindowInfoParsing::testNeedsDeepIconNegative() {
    WindowInfo w;
    w.className = QStringLiteral("firefox");
    QVERIFY(!w.needsDeepIcon());

    w = WindowInfo();
    w.className = QStringLiteral("kitty");
    QVERIFY(!w.needsDeepIcon());

    w = WindowInfo();
    w.className = QStringLiteral("");
    w.title = QStringLiteral("Settings");
    QVERIFY(!w.needsDeepIcon());
}

void TestWindowInfoParsing::testSortOrder() {
    QVector<WindowInfo> windows;
    for (int i = 3; i >= 0; --i) {
        WindowInfo w;
        w.windowId = WindowId{QStringLiteral("0x%1").arg(i + 1, 4, 16, QLatin1Char('0'))};
        w.className = QStringLiteral("App%1").arg(i);
        w.workspaceId = WorkspaceId{QStringLiteral("1")};
        w.focusHistoryId = i;
        windows.append(w);
    }

    std::sort(windows.begin(), windows.end(), [](const WindowInfo &a, const WindowInfo &b) {
        if (a.focusHistoryId != b.focusHistoryId)
            return a.focusHistoryId < b.focusHistoryId;
        return a.displayName < b.displayName;
    });

    for (int i = 0; i < windows.size(); ++i)
        QCOMPARE(windows[i].focusHistoryId, i);
}

void TestWindowInfoParsing::testStableKey() {
    WindowInfo w;
    w.windowId = WindowId{QStringLiteral("0x1234")};
    w.pid = 5678;
    QCOMPARE(w.stableKey(), QStringLiteral("0x1234"));
}

void TestWindowInfoParsing::testWorkspaceIdInt() {
    WindowInfo w;
    w.workspaceId = WorkspaceId{QStringLiteral("3")};
    QCOMPARE(w.workspaceIdInt(), 3);

    w.workspaceId = WorkspaceId{QStringLiteral("-1")};
    QCOMPARE(w.workspaceIdInt(), -1);

    w.workspaceId = WorkspaceId{QStringLiteral("0")};
    QCOMPARE(w.workspaceIdInt(), 0);

    w.workspaceId = WorkspaceId{};
    QCOMPARE(w.workspaceIdInt(), 0);
}

void TestWindowInfoParsing::testJsonInfoRequest()
{
    QCOMPARE(HyprlandRequest::jsonInfoRequest(u"clients"), QByteArrayLiteral("j/clients"));
    QCOMPARE(HyprlandRequest::jsonInfoRequest(u"monitors"), QByteArrayLiteral("j/monitors"));
    QCOMPARE(HyprlandRequest::jsonInfoRequest(u"activewindow"), QByteArrayLiteral("j/activewindow"));
    QCOMPARE(HyprlandRequest::jsonInfoRequest(u"activeworkspace"), QByteArrayLiteral("j/activeworkspace"));
}

void TestWindowInfoParsing::testEvalRequest()
{
    QCOMPARE(HyprlandRequest::evalRequest(u"hl.dispatch(hl.dsp.focus({ workspace = \"3\" }))"),
             QByteArrayLiteral("/eval hl.dispatch(hl.dsp.focus({ workspace = \"3\" }))"));
}

void TestWindowInfoParsing::testFocusWorkspaceRequest()
{
    QCOMPARE(HyprlandRequest::focusWorkspaceRequest(3),
             QByteArrayLiteral("/eval hl.dispatch(hl.dsp.focus({ workspace = \"3\" }))"));
}

void TestWindowInfoParsing::testFocusWindowRequest()
{
    QCOMPARE(HyprlandRequest::focusWindowRequest(QStringLiteral("0x1234")),
             QByteArrayLiteral("/eval hl.dispatch(hl.dsp.focus({ window = \"address:0x1234\" }))"));
}

QTEST_MAIN(TestWindowInfoParsing)
#include "HyprlandWindowSourceTest.moc"
