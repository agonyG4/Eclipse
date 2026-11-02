#include "theme/SystemFontConfig.hpp"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

class SystemFontConfigTest : public QObject {
    Q_OBJECT
private slots:
    void preservesAndRestores() {
        QTemporaryDir dir;
        const QString path = dir.filePath("fonts.conf");
        const QByteArray original = "<?xml version=\"1.0\"?>\n<fontconfig>\n<!-- user emoji/rendering -->\n<alias><family>sans-serif</family><prefer><family>DejaVu Sans</family></prefer></alias>\n</fontconfig>\n";
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(original), original.size()); file.close();
        QString error;
        QVERIFY2(Astrea::SystemFontConfig::apply(path, "Liberation Sans", "DejaVu Sans Mono", &error), qPrintable(error));
        QVERIFY(file.open(QIODevice::ReadOnly)); const auto applied = file.readAll(); file.close();
        QVERIFY(applied.contains("<!-- user emoji/rendering -->"));
        QProcess match;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("FONTCONFIG_FILE", path);
        // Load installed fonts while keeping configuration isolated.
        env.insert("FONTCONFIG_PATH", "/etc/fonts");
        match.setProcessEnvironment(env);
        // Substitute patterns without requiring those families to be installed.
        match.start("fc-pattern", {"--config", "--format", "%{family}", "sans-serif"});
        QVERIFY(match.waitForFinished()); QCOMPARE(match.exitCode(), 0);
        QVERIFY(match.readAllStandardOutput().startsWith("Liberation Sans,"));
        match.start("fc-pattern", {"--config", "--format", "%{family}", "monospace"});
        QVERIFY(match.waitForFinished()); QCOMPARE(match.exitCode(), 0);
        QVERIFY(match.readAllStandardOutput().startsWith("DejaVu Sans Mono,"));
        match.start("fc-pattern", {"--config", "--format", "%{family}", "Explicit App Font"});
        QVERIFY(match.waitForFinished()); QCOMPARE(match.exitCode(), 0);
        QVERIFY(match.readAllStandardOutput().startsWith("Explicit App Font"));
        QFile backup(path + ".before-astrea");
        QVERIFY(backup.open(QIODevice::ReadOnly)); QCOMPARE(backup.readAll(), original);
        QVERIFY(Astrea::SystemFontConfig::apply(path, "Inter & Test", "Mono", &error));
        QVERIFY(file.open(QIODevice::ReadOnly)); const auto second = file.readAll(); file.close();
        QCOMPARE(second.count("<!-- Astrea typography begin -->"), 1);
        QVERIFY(second.contains("Inter &amp; Test"));
        QVERIFY(Astrea::SystemFontConfig::apply(path, {}, {}, &error));
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), original);
    }
    void rejectsMalformedAndInvalid() {
        QTemporaryDir dir; const QString path = dir.filePath("fonts.conf");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("<fontconfig><broken>"); file.close();
        QString error;
        QVERIFY(!Astrea::SystemFontConfig::apply(path, "Inter", "Mono", &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("<fontconfig><broken>")); file.close();
        QVERIFY(!Astrea::SystemFontConfig::apply(dir.filePath("other.conf"), "Bad\nFont", "Mono", &error));
        QVERIFY(!QFile::exists(dir.filePath("other.conf")));
    }
    void refusesSymlinkAndDamagedOwnedBlock() {
        QTemporaryDir dir; QString error;
        const QString path = dir.filePath("fonts.conf");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray damaged = "<fontconfig><!-- Astrea typography begin --></fontconfig>";
        file.write(damaged); file.close();
        QVERIFY(!Astrea::SystemFontConfig::apply(path, "Inter", "Mono", &error));
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), damaged); file.close();
        QVERIFY(QFile::link(path, dir.filePath("link.conf")));
        QVERIFY(!Astrea::SystemFontConfig::apply(dir.filePath("link.conf"), "Inter", "Mono", &error));
    }
    void computesDpi() {
        using Astrea::SystemFontConfig::OutputDensity;
        const OutputDensity desk{"DP-1", {1920, 1080}, {530, 300}, 1.0};
        QCOMPARE(Astrea::SystemFontConfig::fontconfigDpi(desk), 96.0);
        QVERIFY(qAbs(Astrea::SystemFontConfig::physicalPpi(desk) - 91.9) < 0.1);
        QCOMPARE(Astrea::SystemFontConfig::fontconfigDpi({"eDP-1", {2880, 1800}, {300, 190}, 1.75}), 168.0);
        QCOMPARE(Astrea::SystemFontConfig::fontconfigDpi({"eDP-1", {}, {}, 1.25}), 120.0);
        QCOMPARE(Astrea::SystemFontConfig::fontconfigDpi({"x", {}, {}, 0.0}), 96.0);
        QCOMPARE(Astrea::SystemFontConfig::fontconfigDpi({"x", {}, {}, 9.0}), 384.0);
        QCOMPARE(Astrea::SystemFontConfig::physicalPpi({"x", {1920, 1080}, {}, 1.0}), 0.0);
    }
    void parsesEdidSize() {
        QByteArray edid(128, '\0');
        const char header[] = {0, char(0xff), char(0xff), char(0xff), char(0xff), char(0xff), char(0xff), 0};
        edid.replace(0, 8, QByteArray(header, 8));
        edid[21] = 53; edid[22] = 30;
        quint8 sum = 0;
        for (int i = 0; i < 127; ++i) sum += quint8(edid.at(i));
        edid[127] = char(quint8(256 - sum));
        QCOMPARE(Astrea::SystemFontConfig::edidPhysicalSizeMm(edid), QSize(530, 300));
        QByteArray corrupt = edid; corrupt[100] = 1;
        QVERIFY(Astrea::SystemFontConfig::edidPhysicalSizeMm(corrupt).isEmpty());
        QVERIFY(Astrea::SystemFontConfig::edidPhysicalSizeMm(edid.left(64)).isEmpty());
        QByteArray projector = edid; projector[21] = 0; projector[127] = char(quint8(projector[127] + 53));
        QVERIFY(Astrea::SystemFontConfig::edidPhysicalSizeMm(projector).isEmpty());
    }
    void takesOverDpi() {
        QTemporaryDir dir; QString error;
        const QString path = dir.filePath("fonts.conf");
        const QByteArray original =
            "<?xml version='1.0' encoding='UTF-8'?>\n<fontconfig>\n <match target=\"font\">\n"
            "  <edit mode=\"assign\" name=\"hintstyle\">\n   <const>hintslight</const>\n  </edit>\n"
            "  <edit mode=\"assign\" name=\"dpi\">\n   <double>96</double>\n  </edit>\n </match>\n"
            " <match target=\"pattern\">\n  <test name=\"family\" compare=\"contains\"><string>SF Pro</string></test>\n"
            "  <edit name=\"rgba\" mode=\"assign\"><const>none</const></edit>\n </match>\n</fontconfig>\n";
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write(original); file.close();
        const Astrea::SystemFontConfig::OutputDensity output{"DP-1", {2560, 1440}, {600, 340}, 1.5};
        QVERIFY2(Astrea::SystemFontConfig::applyDpi(path, output, &error), qPrintable(error));
        QVERIFY(file.open(QIODevice::ReadOnly)); const auto applied = file.readAll(); file.close();
        QCOMPARE(applied.count("name=\"dpi\""), 2);
        QCOMPARE(applied.count("<double>144</double>"), 2);
        QVERIFY(!applied.contains("<double>96</double>"));
        QVERIFY(applied.contains("<const>hintslight</const>"));
        QVERIFY(applied.contains("<string>SF Pro</string>"));
        QVERIFY(applied.contains("DP-1: 2560x1440 px, scale 1.5, 600x340 mm, 108.2 ppi physical"));
        QFile backup(path + ".before-astrea");
        QVERIFY(backup.open(QIODevice::ReadOnly)); QCOMPARE(backup.readAll(), original);
        // Unchanged density leaves the file alone; typography and display blocks coexist.
        QVERIFY(Astrea::SystemFontConfig::applyDpi(path, output, &error));
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), applied); file.close();
        QVERIFY2(Astrea::SystemFontConfig::apply(path, "Inter", "Mono", &error), qPrintable(error));
        QVERIFY2(Astrea::SystemFontConfig::applyDpi(path, {"DP-1", {1920, 1080}, {}, 1.0}, &error), qPrintable(error));
        QVERIFY(file.open(QIODevice::ReadOnly)); const auto both = file.readAll(); file.close();
        QCOMPARE(both.count("<!-- Astrea display begin -->"), 1);
        QCOMPARE(both.count("<!-- Astrea typography begin -->"), 1);
        QCOMPARE(both.count("<double>96</double>"), 2);
        QVERIFY(!both.contains("ppi physical"));
        QProcess match;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("FONTCONFIG_FILE", path);
        env.insert("FONTCONFIG_PATH", "/etc/fonts");
        match.setProcessEnvironment(env);
        match.start("fc-pattern", {"--config", "--format", "%{dpi}", "sans-serif"});
        QVERIFY(match.waitForFinished()); QCOMPARE(match.exitCode(), 0);
        QCOMPARE(match.readAllStandardOutput(), QByteArray("96"));
    }
    void createsConfig() {
        QTemporaryDir dir; QString error;
        const QString path = dir.filePath("fontconfig/fonts.conf");
        QVERIFY2(Astrea::SystemFontConfig::apply(path, "Inter", "Mono", &error), qPrintable(error));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); QVERIFY(file.readAll().contains("<fontconfig>"));
    }
};
QTEST_GUILESS_MAIN(SystemFontConfigTest)
#include "SystemFontConfigTest.moc"
