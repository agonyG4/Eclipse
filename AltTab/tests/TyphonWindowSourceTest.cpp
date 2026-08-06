#include <QSignalSpy>
#include <QTest>

#include "platform/typhon/TyphonWindowSource.hpp"

using namespace Astrea::Typhon;

class TyphonWindowSourceTest final : public QObject {
    Q_OBJECT

private slots:
    void descriptorAndCapabilities();
    void snapshotMappingPreservesTyphonIdentity();
    void activeAndMinimizedFlagsMap();
    void unsupportedActivationIsDeterministic();
};

void TyphonWindowSourceTest::descriptorAndCapabilities()
{
    TyphonWindowSource source;
    QCOMPARE(source.descriptor().name, QStringLiteral("typhon"));
    QCOMPARE(source.descriptor().protocolVersion, 1);
    const auto capabilities = source.capabilities();
    QVERIFY(capabilities.contains(BackendCapability::WindowList));
    QVERIFY(capabilities.contains(BackendCapability::EventStream));
    QVERIFY(capabilities.contains(BackendCapability::ActiveWindow));
    QVERIFY(!capabilities.contains(BackendCapability::WindowActivation));
    QVERIFY(!capabilities.contains(BackendCapability::ActiveOutput));
}

void TyphonWindowSourceTest::snapshotMappingPreservesTyphonIdentity()
{
    TyphonWindowSource source;
    const Toplevel window{QStringLiteral("18446744073709551615"), QStringLiteral("org.example.App"),
                          QStringLiteral("Example"), 42, ToplevelKind::XdgToplevel, {}, 0, 0};
    const Snapshot snapshot{{window}, 3, 1, false, 8};
    const auto mapped = source.mapSnapshotForTest(snapshot);
    QCOMPARE(mapped.windows.size(), 1);
    QCOMPARE(mapped.windows.first().windowId.value, window.id);
    QCOMPARE(mapped.windows.first().pid, qint64(42));
    QCOMPARE(mapped.windows.first().className, window.appId);
    QCOMPARE(mapped.windows.first().initialClass, QString());
    QCOMPARE(mapped.windows.first().title, window.title);
    QCOMPARE(mapped.windows.first().initialTitle, window.title);
    QCOMPARE(mapped.windows.first().backendGeneration, quint64(8));
    QCOMPARE(mapped.windows.first().focusHistoryId, 1000000);
}

void TyphonWindowSourceTest::activeAndMinimizedFlagsMap()
{
    TyphonWindowSource source;
    Toplevel window;
    window.id = QStringLiteral("1");
    window.appId = QStringLiteral("app");
    window.title = QStringLiteral("title");
    window.states = ToplevelStates(ToplevelStateFlag::Active) | ToplevelStateFlag::Minimized;
    const auto mapped = source.mapSnapshotForTest(Snapshot{{window}, 1, 1, false, 1});
    QVERIFY(mapped.windows.first().isActive);
    QVERIFY(mapped.windows.first().isMinimized);
    QVERIFY(mapped.windows.first().isHidden);
    QCOMPARE(mapped.activeWindowId.value, QStringLiteral("1"));
}

void TyphonWindowSourceTest::unsupportedActivationIsDeterministic()
{
    TyphonWindowSource source;
    QSignalSpy activationSpy(&source, &CompositorBackend::activationFinished);
    source.activateWindow({WindowId{QStringLiteral("1")}, 9});
    QCOMPARE(activationSpy.count(), 1);
    const ActivationResult result = activationSpy.at(0).at(1).value<ActivationResult>();
    QVERIFY(!result.success);
    QCOMPARE(result.error, QStringLiteral("Typhon window activation is unsupported"));
}

QTEST_MAIN(TyphonWindowSourceTest)
#include "TyphonWindowSourceTest.moc"
