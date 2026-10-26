#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QTemporaryFile>
#include <QTest>
#include <fcntl.h>
#include <unistd.h>

#include "platform/typhon/TyphonWindowIcon.hpp"

class TyphonWindowIconTest final : public QObject {
    Q_OBJECT
private slots:
    void decodesPixelsFromOffsetZeroAndClosesFd();
    void rejectsOversizedAndTruncatedPayloads();
    void rejectsPipesWithoutWaitingForData();
};

void TyphonWindowIconTest::decodesPixelsFromOffsetZeroAndClosesFd()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    const quint32 pixels[] = {0xffff0000, 0xff00ff00, 0xff0000ff, 0x80800000};
    QCOMPARE(file.write(reinterpret_cast<const char *>(pixels), sizeof(pixels)), qint64(sizeof(pixels)));
    QVERIFY(file.flush());
    const int fd = ::dup(file.handle());
    QVERIFY(fd >= 0);
    const QString source = Astrea::Typhon::windowIconFromPixels(fd, 2);
    QCOMPARE(::fcntl(fd, F_GETFD), -1);
    QVERIFY(source.startsWith(QStringLiteral("data:image/png;base64,")));
    const QByteArray png = QByteArray::fromBase64(source.mid(source.indexOf(QLatin1Char(',')) + 1).toLatin1());
    const QImage image = QImage::fromData(png, "PNG");
    QCOMPARE(image.size(), QSize(2, 2));
    QCOMPARE(image.pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(image.pixelColor(1, 1).alpha(), 128);
}

void TyphonWindowIconTest::rejectsOversizedAndTruncatedPayloads()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write("bad", 3);
    QVERIFY(file.flush());
    QVERIFY(Astrea::Typhon::windowIconFromPixels(::dup(file.handle()), 2).isEmpty());
    QVERIFY(Astrea::Typhon::windowIconFromPixels(::dup(file.handle()), 257).isEmpty());
    QVERIFY(Astrea::Typhon::windowIconFromPixels(::dup(file.handle()), 0).isEmpty());
}

void TyphonWindowIconTest::rejectsPipesWithoutWaitingForData()
{
    int fds[2];
    QCOMPARE(::pipe(fds), 0);
    QVERIFY(Astrea::Typhon::windowIconFromPixels(fds[0], 2).isEmpty());
    QCOMPARE(::fcntl(fds[0], F_GETFD), -1);
    ::close(fds[1]);
}

QTEST_GUILESS_MAIN(TyphonWindowIconTest)
#include "TyphonWindowIconTest.moc"
