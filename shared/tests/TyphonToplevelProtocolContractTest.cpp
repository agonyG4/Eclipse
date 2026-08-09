#include <QCryptographicHash>
#include <QFile>
#include <QTest>

class TyphonToplevelProtocolContractTest final : public QObject {
    Q_OBJECT

private slots:
    void fixtureMatchesTyphonM7BContract();
};

void TyphonToplevelProtocolContractTest::fixtureMatchesTyphonM7BContract()
{
    QFile file(QStringLiteral(ASTREA_TOPLEVEL_PROTOCOL_SOURCE_FILE));
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
    const QByteArray xml = file.readAll();

    QCOMPARE(QCryptographicHash::hash(xml, QCryptographicHash::Sha256).toHex(),
             QByteArrayLiteral("0dd3449fda60b1ed183e330e1589093f3d4f8086be117d9ca4baa81bd6bd47e7"));
    QVERIFY(xml.contains("<interface name=\"astrea_toplevel_manager_v1\" version=\"2\">"));
    QVERIFY(xml.contains("<interface name=\"astrea_toplevel_v1\" version=\"2\">"));
    QVERIFY(xml.contains("<request name=\"activate\" since=\"2\">"));
    QVERIFY(xml.contains("<request name=\"minimize\" since=\"2\">"));
    QVERIFY(xml.contains("<request name=\"restore\" since=\"2\">"));
    QVERIFY(xml.contains("<request name=\"close\" since=\"2\">"));
    QVERIFY(xml.contains("<event name=\"action_done\" since=\"2\">"));
    QVERIFY(xml.contains("<entry name=\"accepted\" value=\"0\"/>"));
    QVERIFY(xml.contains("<entry name=\"no_change\" value=\"1\"/>"));
    QVERIFY(xml.contains("<entry name=\"unavailable\" value=\"2\"/>"));
}

QTEST_GUILESS_MAIN(TyphonToplevelProtocolContractTest)
#include "TyphonToplevelProtocolContractTest.moc"
