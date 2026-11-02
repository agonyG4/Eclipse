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
    void createsConfig() {
        QTemporaryDir dir; QString error;
        const QString path = dir.filePath("fontconfig/fonts.conf");
        QVERIFY2(Astrea::SystemFontConfig::apply(path, "Inter", "Mono", &error), qPrintable(error));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); QVERIFY(file.readAll().contains("<fontconfig>"));
    }
};
QTEST_GUILESS_MAIN(SystemFontConfigTest)
#include "SystemFontConfigTest.moc"
