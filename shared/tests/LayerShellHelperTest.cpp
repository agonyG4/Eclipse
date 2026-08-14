#include "platform/wayland/LayerShellHelper.hpp"

#include <QTest>

class LayerShellHelperTest final : public QObject {
    Q_OBJECT

private slots:
    void compiledMatchesBuildCapability();
    void prepareReportsBuildCapability();
};

void LayerShellHelperTest::compiledMatchesBuildCapability()
{
#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
    QVERIFY(AstreaLayerShellHelper::compiled());
#else
    QVERIFY(!AstreaLayerShellHelper::compiled());
#endif
}

void LayerShellHelperTest::prepareReportsBuildCapability()
{
    QString error;
    const bool prepared = AstreaLayerShellHelper::prepare(&error);
#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
    QVERIFY2(prepared, qPrintable(error));
    QVERIFY(error.isEmpty());
#else
    QVERIFY(!prepared);
    QVERIFY(error.contains(QStringLiteral("LayerShellQt")));
#endif
}

QTEST_GUILESS_MAIN(LayerShellHelperTest)
#include "LayerShellHelperTest.moc"
