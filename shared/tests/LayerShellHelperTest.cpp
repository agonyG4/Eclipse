#include "platform/wayland/LayerShellHelper.hpp"

#include <QTest>

class LayerShellHelperTest final : public QObject {
    Q_OBJECT

private slots:
    void compiledMatchesBuildCapability();
    void runtimeCapabilityRequiresWaylandAndProtocol();
};

void LayerShellHelperTest::compiledMatchesBuildCapability()
{
#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
    QVERIFY(AstreaLayerShellHelper::compiled());
#else
    QVERIFY(!AstreaLayerShellHelper::compiled());
#endif
}

void LayerShellHelperTest::runtimeCapabilityRequiresWaylandAndProtocol()
{
    QString error;
    const bool capable = AstreaLayerShellHelper::validateRuntime(true, true, &error);
#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
    QVERIFY2(capable, qPrintable(error));
    QVERIFY(error.isEmpty());
#else
    QVERIFY(!capable);
    QVERIFY(error.contains(QStringLiteral("LayerShellQt")));
#endif

    error.clear();
    QVERIFY(!AstreaLayerShellHelper::validateRuntime(true, false, &error));
#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
    QVERIFY(error.contains(QStringLiteral("zwlr_layer_shell_v1")));
#else
    QVERIFY(error.contains(QStringLiteral("LayerShellQt")));
#endif

    error.clear();
    QVERIFY(!AstreaLayerShellHelper::validateRuntime(false, true, &error));
#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
    QVERIFY(error.contains(QStringLiteral("Wayland")));
#else
    QVERIFY(error.contains(QStringLiteral("LayerShellQt")));
#endif
}

QTEST_GUILESS_MAIN(LayerShellHelperTest)
#include "LayerShellHelperTest.moc"
