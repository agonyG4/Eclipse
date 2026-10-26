#include <QCryptographicHash>
#include <QFile>
#include <QTest>

class TyphonToplevelProtocolContractTest final : public QObject {
    Q_OBJECT

private slots:
    void fixtureMatchesTyphonIconContract();
};

void TyphonToplevelProtocolContractTest::fixtureMatchesTyphonIconContract()
{
    QFile file(QStringLiteral(ASTREA_TOPLEVEL_PROTOCOL_SOURCE_FILE));
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
    const QByteArray xml = file.readAll();

    QCOMPARE(QCryptographicHash::hash(xml, QCryptographicHash::Sha256).toHex(),
             QByteArrayLiteral("9ea1dd205ac10470a84c685811be52045e890ec376f9f95ad766a5ba689f42c5"));
    QVERIFY(xml.contains("<interface name=\"astrea_toplevel_manager_v1\" version=\"4\">"));
    QVERIFY(xml.contains("<interface name=\"astrea_toplevel_v1\" version=\"4\">"));
    QVERIFY(xml.contains("<request name=\"activate\" since=\"2\">"));
    QVERIFY(xml.contains("<request name=\"minimize\" since=\"2\">"));
    QVERIFY(xml.contains("<request name=\"restore\" since=\"2\">"));
    QVERIFY(xml.contains("<request name=\"close\" since=\"2\">"));
    QVERIFY(xml.contains("<event name=\"action_done\" since=\"2\">"));
    QVERIFY(xml.contains("<request name=\"set_minimize_anchor\" since=\"3\">"));
    QVERIFY(xml.contains("<request name=\"clear_minimize_anchor\" since=\"3\"/>"));
    QVERIFY(xml.contains("<entry name=\"invalid_anchor\" value=\"3\" since=\"3\"/>"));
    QVERIFY(xml.contains("<event name=\"icon_name\" since=\"4\">"));
    QVERIFY(xml.contains("<event name=\"icon_pixels\" since=\"4\">"));
    QVERIFY(xml.contains("<entry name=\"accepted\" value=\"0\"/>"));
    QVERIFY(xml.contains("<entry name=\"no_change\" value=\"1\"/>"));
    QVERIFY(xml.contains("<entry name=\"unavailable\" value=\"2\"/>"));
}

QTEST_GUILESS_MAIN(TyphonToplevelProtocolContractTest)
#include "TyphonToplevelProtocolContractTest.moc"
