#include <QProcessEnvironment>
#include <QTest>

#include "platform/runtime/DockRuntimePaths.hpp"

class DockRuntimePathsTest final : public QObject {
    Q_OBJECT

private slots:
    void usesAstreaRootFromEnvironment();
};

void DockRuntimePathsTest::usesAstreaRootFromEnvironment()
{
    const QByteArray previous = qgetenv("ASTREA_ROOT");
    qputenv("ASTREA_ROOT", "/tmp/astrea-test");
    const DockRuntimePaths paths = DockRuntimePaths::fromEnvironment();
    if (previous.isNull())
        qunsetenv("ASTREA_ROOT");
    else
        qputenv("ASTREA_ROOT", previous);

    QCOMPARE(paths.astreaRoot(), QStringLiteral("/tmp/astrea-test"));
    QCOMPARE(paths.astreaLaunch(), QStringLiteral("/tmp/astrea-test/bin/astrea-launch"));
    QVERIFY(paths.dockConfigPath().endsWith(QStringLiteral("/.config/AstreaOS/dock.json")));
    QVERIFY(paths.componentsConfigPath().endsWith(QStringLiteral("/.config/AstreaOS/ui/components.json")));
}

QTEST_MAIN(DockRuntimePathsTest)
#include "DockRuntimePathsTest.moc"
