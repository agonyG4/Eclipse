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
             QByteArrayLiteral("becf19ed870a6cd7cfd146821a594bd0b4810326b121880c06866a51ce9252d0"));
    QVERIFY(xml.contains("<interface name=\"astrea_toplevel_manager_v1\" version=\"3\">"));
    QVERIFY(xml.contains("<interface name=\"astrea_toplevel_v1\" version=\"3\">"));
    QVERIFY(xml.contains("<request name=\"activate\" since=\"2\">"));
    QVERIFY(xml.contains("<request name=\"minimize\" since=\"2\">"));
    QVERIFY(xml.contains("<request name=\"restore\" since=\"2\">"));
    QVERIFY(xml.contains("<request name=\"close\" since=\"2\">"));
    QVERIFY(xml.contains("<event name=\"action_done\" since=\"2\">"));
    QVERIFY(xml.contains("<request name=\"set_minimize_anchor\" since=\"3\">"));
    QVERIFY(xml.contains("<request name=\"clear_minimize_anchor\" since=\"3\"/>"));
    QVERIFY(xml.contains("<entry name=\"invalid_anchor\" value=\"3\" since=\"3\"/>"));
    QVERIFY(xml.contains("<entry name=\"accepted\" value=\"0\"/>"));
    QVERIFY(xml.contains("<entry name=\"no_change\" value=\"1\"/>"));
    QVERIFY(xml.contains("<entry name=\"unavailable\" value=\"2\"/>"));
}

QTEST_GUILESS_MAIN(TyphonToplevelProtocolContractTest)
#include "TyphonToplevelProtocolContractTest.moc"
