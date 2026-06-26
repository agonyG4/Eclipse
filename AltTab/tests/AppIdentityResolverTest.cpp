#include <QTest>
#include <QSignalSpy>

#include "services/AppIdentityResolver.hpp"

class TestAppIdentityResolver : public QObject {
    Q_OBJECT

private slots:
    void testSoberAlias();
    void testZenAlias();
    void testKittyAlias();
    void testVsCodeAlias();
    void testSpotifyAlias();
    void testDiscordAlias();
    void testSteamAppId();
    void testSteamAppDefault();
    void testObsNotObsidian();
    void testObsidianNotObs();
    void testDeepPendingForExe();
    void testDeepPendingForProton();
    void testAsyncResolution();
};

void TestAppIdentityResolver::testSoberAlias() {
    AppIdentityResolver resolver;
    WindowIdentityInput input;
    input.className = QStringLiteral("org.vinegarhq.sober");
    input.initialClass = QStringLiteral("org.vinegarhq.sober");

    AppIdentity identity = resolver.resolveSync(input);
    QCOMPARE(identity.iconName, QStringLiteral("org.vinegarhq.Sober"));
}

void TestAppIdentityResolver::testZenAlias() {
    AppIdentityResolver resolver;
    WindowIdentityInput input;
    input.className = QStringLiteral("zen-browser");

    AppIdentity identity = resolver.resolveSync(input);
    QCOMPARE(identity.iconName, QStringLiteral("zen-browser"));
}

void TestAppIdentityResolver::testKittyAlias() {
    AppIdentityResolver resolver;
    WindowIdentityInput input;
    input.className = QStringLiteral("kitty");

    AppIdentity identity = resolver.resolveSync(input);
    QCOMPARE(identity.iconName, QStringLiteral("kitty"));
}

void TestAppIdentityResolver::testVsCodeAlias() {
    AppIdentityResolver resolver;
    WindowIdentityInput input;
    input.className = QStringLiteral("code-oss");

    AppIdentity identity = resolver.resolveSync(input);
    QCOMPARE(identity.iconName, QStringLiteral("visual-studio-code"));
}

void TestAppIdentityResolver::testSpotifyAlias() {
    AppIdentityResolver resolver;
    WindowIdentityInput input;
    input.className = QStringLiteral("spotify");

    AppIdentity identity = resolver.resolveSync(input);
    QCOMPARE(identity.iconName, QStringLiteral("spotify"));
}

void TestAppIdentityResolver::testDiscordAlias() {
    AppIdentityResolver resolver;
    WindowIdentityInput input;
    input.className = QStringLiteral("discord");

    AppIdentity identity = resolver.resolveSync(input);
    QCOMPARE(identity.iconName, QStringLiteral("discord"));
}

void TestAppIdentityResolver::testSteamAppId() {
    AppIdentityResolver resolver;
    WindowIdentityInput input;
    input.className = QStringLiteral("steam_app_12345");
    input.title = QStringLiteral("Counter-Strike 2");

    AppIdentity identity = resolver.resolveSync(input);
    QCOMPARE(identity.iconName, QStringLiteral("steam_icon_12345"));

    // Check that displayName is set from title
    QCOMPARE(identity.displayName, QStringLiteral("Counter-Strike 2"));
}

void TestAppIdentityResolver::testSteamAppDefault() {
    AppIdentityResolver resolver;
    WindowIdentityInput input;
    input.className = QStringLiteral("steam_app_default");

    AppIdentity identity = resolver.resolveSync(input);
    // Should NOT resolve to generic steam icon
    QVERIFY(identity.iconName.isEmpty() || identity.iconName != QStringLiteral("steam"));
    QVERIFY(identity.iconPending);
    QVERIFY(!identity.showFallbackText);
}

void TestAppIdentityResolver::testObsNotObsidian() {
    AppIdentityResolver resolver;

    // OBS Studio
    WindowIdentityInput obsInput;
    obsInput.className = QStringLiteral("obs");
    AppIdentity obsIdentity = resolver.resolveSync(obsInput);
    QCOMPARE(obsIdentity.iconName, QStringLiteral("com.obsproject.Studio"));

    // Obsidian
    WindowIdentityInput obsidianInput;
    obsidianInput.className = QStringLiteral("obsidian");
    AppIdentity obsidianIdentity = resolver.resolveSync(obsidianInput);
    QCOMPARE(obsidianIdentity.iconName, QStringLiteral("obsidian"));
}

void TestAppIdentityResolver::testObsidianNotObs() {
    AppIdentityResolver resolver;
    WindowIdentityInput input;
    input.className = QStringLiteral("obsidian");

    AppIdentity identity = resolver.resolveSync(input);
    QCOMPARE(identity.iconName, QStringLiteral("obsidian"));
    QVERIFY(identity.iconName != QStringLiteral("com.obsproject.Studio"));
}

void TestAppIdentityResolver::testDeepPendingForExe() {
    AppIdentityResolver resolver;
    WindowIdentityInput input;
    input.className = QStringLiteral("XxX_NONEXISTENT_APP_XxX.exe");
    input.title = QStringLiteral("Some deep window title");
    input.address = QStringLiteral("0x1234");

    AppIdentity identity = resolver.resolveSync(input);
    QVERIFY(identity.iconPending);
    QVERIFY(!identity.showFallbackText);
}

void TestAppIdentityResolver::testDeepPendingForProton() {
    AppIdentityResolver resolver;
    WindowIdentityInput input;
    input.className = QStringLiteral("steam_app_12345");
    input.initialClass = QStringLiteral("proton");
    input.title = QStringLiteral("Game via Proton");
    input.address = QStringLiteral("0x5678");

    AppIdentity identity = resolver.resolveSync(input);
    // steam_app_ should resolve, but also proton deep resolve is pending
    QCOMPARE(identity.iconName, QStringLiteral("steam_icon_12345"));
}

void TestAppIdentityResolver::testAsyncResolution() {
    AppIdentityResolver resolver;
    QSignalSpy spy(&resolver, &AppIdentityResolver::identityResolved);

    WindowIdentityInput input;
    input.className = QStringLiteral("kitty");
    input.address = QStringLiteral("0x9999");

    resolver.resolveAsync(input, 1);

    QCOMPARE(spy.count(), 1);
    const QString address = spy.at(0).at(0).toString();
    const AppIdentity identity = spy.at(0).at(1).value<AppIdentity>();
    QCOMPARE(address, QStringLiteral("0x9999"));
    QCOMPARE(identity.iconName, QStringLiteral("kitty"));
}

QTEST_MAIN(TestAppIdentityResolver)
#include "AppIdentityResolverTest.moc"
