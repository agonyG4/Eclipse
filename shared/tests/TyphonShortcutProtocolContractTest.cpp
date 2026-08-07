#include <QCryptographicHash>
#include <QFile>
#include <QTest>

class TyphonShortcutProtocolContractTest final : public QObject {
    Q_OBJECT

private slots:
    void fixtureMatchesCommittedProtocol();
};

void TyphonShortcutProtocolContractTest::fixtureMatchesCommittedProtocol()
{
    QFile file(QStringLiteral(ASTREA_SHORTCUT_PROTOCOL_SOURCE_FILE));
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
    const QByteArray xml = file.readAll();

    QCOMPARE(QCryptographicHash::hash(xml, QCryptographicHash::Sha256).toHex(),
             QByteArrayLiteral("4c3999e91e088b3ef5cc57245e9fac544929a811b097f8964d05c0268265867e"));
    QVERIFY(xml.contains("interface name=\"astrea_shortcuts_manager_v1\""));
    QVERIFY(xml.contains("interface name=\"astrea_shortcut_v1\""));
    QVERIFY(xml.contains("<event name=\"pressed\">"));
    QVERIFY(xml.contains("<event name=\"repeated\">"));
    QVERIFY(xml.contains("<event name=\"released\">"));
    QVERIFY(xml.contains("<event name=\"cancelled\">"));
}

QTEST_GUILESS_MAIN(TyphonShortcutProtocolContractTest)
#include "TyphonShortcutProtocolContractTest.moc"
