#include "theme/FontconfigDpiSync.hpp"
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QScreen>
#include <QTemporaryDir>
#include <QTest>
#include <QWindow>

class FontconfigDpiSyncTest : public QObject {
    Q_OBJECT
private slots:
    void writesPrimaryOutputDpi() {
        QTemporaryDir dir; QString error;
        const QString path = dir.filePath("fontconfig/fonts.conf");
        Astrea::FontconfigDpiSync sync(path, dir.filePath("drm"));
        QScreen *screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const auto density = sync.densityFor(screen);
        QCOMPARE(density.scale, screen->devicePixelRatio());
        QCOMPARE(density.pixels, screen->geometry().size() * screen->devicePixelRatio());
        QVERIFY2(sync.syncNow(&error), qPrintable(error));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray expected = "<double>" + QByteArray::number(96 * screen->devicePixelRatio()) + "</double>";
        QVERIFY2(file.readAll().contains(expected), expected.constData());
        file.close();
        // A repeated sync with the same output does not touch the file.
        QVERIFY(file.remove());
        QVERIFY(sync.syncNow(&error));
        QVERIFY(!file.exists());
    }
    void readsConnectorEdid() {
        QTemporaryDir dir;
        QVERIFY(QDir().mkpath(dir.filePath("drm/card1-DP-1")));
        QVERIFY(QDir().mkpath(dir.filePath("drm/card1-DP-10")));
        QByteArray edid(128, '\0');
        const char header[] = {0, char(0xff), char(0xff), char(0xff), char(0xff), char(0xff), char(0xff), 0};
        edid.replace(0, 8, QByteArray(header, 8));
        edid[21] = 53; edid[22] = 30;
        quint8 sum = 0;
        for (int i = 0; i < 127; ++i) sum += quint8(edid.at(i));
        edid[127] = char(quint8(256 - sum));
        QFile file(dir.filePath("drm/card1-DP-1/edid"));
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write(edid); file.close();
        QCOMPARE(Astrea::FontconfigDpiSync::edidSizeMm(dir.filePath("drm"), "DP-1"), QSize(530, 300));
        QVERIFY(Astrea::FontconfigDpiSync::edidSizeMm(dir.filePath("drm"), "DP-10").isEmpty());
        QVERIFY(Astrea::FontconfigDpiSync::edidSizeMm(dir.filePath("drm"), "HDMI-A-1").isEmpty());
        QVERIFY(Astrea::FontconfigDpiSync::edidSizeMm(dir.filePath("drm"), {}).isEmpty());
        // The live connector, when present, reads the same way.
        const QSize live = Astrea::FontconfigDpiSync::edidSizeMm("/sys/class/drm", "DP-1");
        if (!live.isEmpty()) QVERIFY(live.width() > 0 && live.height() > 0);
    }
    void followsWindowScale() {
        QTemporaryDir dir;
        Astrea::FontconfigDpiSync sync(dir.filePath("fonts.conf"), dir.filePath("drm"));
        QWindow window;
        window.resize(64, 64);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCOMPARE(sync.densityFor(window.screen()).scale, window.devicePixelRatio());
    }
};
QTEST_MAIN(FontconfigDpiSyncTest)
#include "FontconfigDpiSyncTest.moc"
