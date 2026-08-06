#include <QProcessEnvironment>
#include <QTest>

#include "platform/compositor/CompositorBackendFactory.hpp"

class CompositorBackendFactoryTest final : public QObject {
    Q_OBJECT

private slots:
    void explicitTyphonCreatesReadOnlyBackend();
    void explicitHyprlandCreatesHyprlandBackend();
    void autoFallsBackToHyprlandWhenTyphonIsNotCompiled();
    void autoReportsUnsupportedWhenNeitherIsAvailable();
};

void CompositorBackendFactoryTest::explicitTyphonCreatesReadOnlyBackend()
{
    std::unique_ptr<CompositorBackend> backend(
        CompositorBackendFactory::createBackend(QStringLiteral("typhon")));
    QVERIFY(backend);
    QCOMPARE(backend->descriptor().name, QStringLiteral("typhon"));
    QVERIFY(!backend->capabilities().contains(BackendCapability::WindowActivation));
}

void CompositorBackendFactoryTest::explicitHyprlandCreatesHyprlandBackend()
{
    std::unique_ptr<CompositorBackend> backend(
        CompositorBackendFactory::createBackend(QStringLiteral("hyprland")));
    QVERIFY(backend);
    QCOMPARE(backend->descriptor().name, QStringLiteral("hyprland"));
}

void CompositorBackendFactoryTest::autoFallsBackToHyprlandWhenTyphonIsNotCompiled()
{
    const QByteArray previous = qgetenv("HYPRLAND_INSTANCE_SIGNATURE");
    qputenv("HYPRLAND_INSTANCE_SIGNATURE", QByteArrayLiteral("factory-test"));
    std::unique_ptr<CompositorBackend> backend(
        CompositorBackendFactory::createBackend(QStringLiteral("auto")));
    if (previous.isNull())
        qunsetenv("HYPRLAND_INSTANCE_SIGNATURE");
    else
        qputenv("HYPRLAND_INSTANCE_SIGNATURE", previous);

#if ASTREA_HAVE_TYPHON_PROTOCOL
    QSKIP("The finalized protocol adapter is compiled in this build");
#else
    QVERIFY(backend);
    QCOMPARE(backend->descriptor().name, QStringLiteral("hyprland"));
#endif
}

void CompositorBackendFactoryTest::autoReportsUnsupportedWhenNeitherIsAvailable()
{
    const QByteArray previous = qgetenv("HYPRLAND_INSTANCE_SIGNATURE");
    qunsetenv("HYPRLAND_INSTANCE_SIGNATURE");
    std::unique_ptr<CompositorBackend> backend(
        CompositorBackendFactory::createBackend(QStringLiteral("auto")));
    if (!previous.isNull())
        qputenv("HYPRLAND_INSTANCE_SIGNATURE", previous);

#if ASTREA_HAVE_TYPHON_PROTOCOL
    QVERIFY(backend);
#else
    QVERIFY(!backend);
#endif
}

QTEST_MAIN(CompositorBackendFactoryTest)
#include "CompositorBackendFactoryTest.moc"
