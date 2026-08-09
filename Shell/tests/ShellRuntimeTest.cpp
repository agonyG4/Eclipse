#include <QCoreApplication>
#include <QTest>

#include "AltTab/core/AltTabController.hpp"
#include "Dock/core/DockController.hpp"
#include "Spotlight/core/SpotlightController.hpp"
#include "runtime/ShellRuntime.hpp"

class ShellRuntimeTest final : public QObject {
    Q_OBJECT

private slots:
    void createsOneSharedOwnershipGraph();
};

void ShellRuntimeTest::createsOneSharedOwnershipGraph()
{
    ShellRuntime runtime;
    QString error;
    QVERIFY2(runtime.initialize(QStringLiteral("auto"), &error), qPrintable(error));

    QVERIFY(runtime.catalog());
    QVERIFY(runtime.identityResolver());
    QVERIFY(runtime.launcher());
    QVERIFY(runtime.typhonSession());
    QVERIFY(runtime.shortcutClient());
    QVERIFY(runtime.windowBackend());
    QVERIFY(runtime.ipcServer());
    QVERIFY(runtime.dockConfig());
    QVERIFY(runtime.altTabConfig());
    QVERIFY(runtime.spotlightConfig());
    QVERIFY(runtime.dockController());
    QVERIFY(runtime.altTabController());
    QVERIFY(runtime.spotlightController());
    QCOMPARE(runtime.dockController()->catalog(), runtime.catalog());
    QCOMPARE(runtime.spotlightController()->catalog(), runtime.catalog());
    QCOMPARE(runtime.spotlightController()->launcher(), runtime.launcher());
    QCOMPARE(runtime.altTabController()->identityResolver(), runtime.identityResolver());
}

QTEST_GUILESS_MAIN(ShellRuntimeTest)
#include "ShellRuntimeTest.moc"
