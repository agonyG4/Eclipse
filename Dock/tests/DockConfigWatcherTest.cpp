#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>

#include "services/DockConfigWatcher.hpp"

class DockConfigWatcherTest final : public QObject {
    Q_OBJECT

private slots:
    void missingConfigUsesDefaults();
    void validConfigIsParsed();
    void invalidFieldsFallbackIndependently();
    void valuesAreClamped();
    void extremeNumbersAreClampedSafely();
    void nonFiniteMagnificationFallsBack();
    void invalidPinsAndDuplicatesAreRejected();
    void oversizedInputUsesDefaults();
    void atomicReplacementReloadsAfterDebounce();
    void componentToggleAndWatcherRecovery();
};

static QString configPath(const QTemporaryDir &dir)
{
    return dir.path() + QStringLiteral("/config/AstreaOS/dock.json");
}

static QString componentsPath(const QTemporaryDir &dir)
{
    return dir.path() + QStringLiteral("/config/AstreaOS/ui/components.json");
}

static void writeJson(const QString &path, const QJsonObject &object)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QSaveFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    QVERIFY(file.commit());
}

void DockConfigWatcherTest::missingConfigUsesDefaults()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DockConfigWatcher watcher(configPath(dir), componentsPath(dir));
    const DockConfig config = watcher.config();
    QCOMPARE(config.iconSize, 48);
    QCOMPARE(config.bottomMargin, 12);
    QCOMPARE(config.panelPadding, 14);
    QCOMPARE(config.itemSpacing, 10);
    QVERIFY(config.magnificationEnabled);
    QCOMPARE(config.magnificationScale, 1.6);
    QCOMPARE(config.magnificationRadius, 2.5);
    QVERIFY(config.pins.isEmpty());
    QVERIFY(watcher.componentEnabled());
}

void DockConfigWatcherTest::validConfigIsParsed()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeJson(configPath(dir), QJsonObject{
        {QStringLiteral("iconSize"), 56},
        {QStringLiteral("bottomMargin"), 20},
        {QStringLiteral("panelPadding"), 18},
        {QStringLiteral("itemSpacing"), 12},
        {QStringLiteral("magnificationEnabled"), false},
        {QStringLiteral("magnificationScale"), 1.8},
        {QStringLiteral("magnificationRadius"), 3.25},
        {QStringLiteral("pins"), QJsonArray{QStringLiteral("firefox.desktop")}}
    });
    DockConfigWatcher watcher(configPath(dir), componentsPath(dir));
    QCOMPARE(watcher.config().iconSize, 56);
    QVERIFY(!watcher.config().magnificationEnabled);
    QCOMPARE(watcher.config().magnificationScale, 1.8);
    QCOMPARE(watcher.config().magnificationRadius, 3.25);
    QCOMPARE(watcher.config().pins, QStringList{QStringLiteral("firefox.desktop")});
}

void DockConfigWatcherTest::invalidFieldsFallbackIndependently()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeJson(configPath(dir), QJsonObject{
        {QStringLiteral("iconSize"), 60},
        {QStringLiteral("bottomMargin"), QStringLiteral("bad")},
        {QStringLiteral("panelPadding"), QJsonObject{}},
        {QStringLiteral("magnificationEnabled"), QStringLiteral("bad")},
        {QStringLiteral("magnificationScale"), QStringLiteral("bad")},
        {QStringLiteral("magnificationRadius"), QJsonObject{}},
        {QStringLiteral("pins"), QJsonArray{QStringLiteral("ok.desktop"), 4}}
    });
    DockConfigWatcher watcher(configPath(dir), componentsPath(dir));
    QCOMPARE(watcher.config().iconSize, 60);
    QCOMPARE(watcher.config().bottomMargin, 12);
    QCOMPARE(watcher.config().panelPadding, 14);
    QVERIFY(watcher.config().magnificationEnabled);
    QCOMPARE(watcher.config().magnificationScale, 1.6);
    QCOMPARE(watcher.config().magnificationRadius, 2.5);
    QCOMPARE(watcher.config().pins, QStringList{QStringLiteral("ok.desktop")});
}

void DockConfigWatcherTest::valuesAreClamped()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeJson(configPath(dir), QJsonObject{
        {QStringLiteral("iconSize"), 1000},
        {QStringLiteral("bottomMargin"), -8},
        {QStringLiteral("panelPadding"), 1000},
        {QStringLiteral("itemSpacing"), 0},
        {QStringLiteral("magnificationScale"), 10.0},
        {QStringLiteral("magnificationRadius"), 0.0}
    });
    DockConfigWatcher watcher(configPath(dir), componentsPath(dir));
    QCOMPARE(watcher.config().iconSize, 64);
    QCOMPARE(watcher.config().bottomMargin, 0);
    QCOMPARE(watcher.config().panelPadding, 32);
    QCOMPARE(watcher.config().itemSpacing, 4);
    QCOMPARE(watcher.config().magnificationScale, 2.0);
    QCOMPARE(watcher.config().magnificationRadius, 1.0);
}

void DockConfigWatcherTest::extremeNumbersAreClampedSafely()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeJson(configPath(dir), QJsonObject{
        {QStringLiteral("iconSize"), 1e100},
        {QStringLiteral("bottomMargin"), -1e100},
        {QStringLiteral("panelPadding"), 20.5},
        {QStringLiteral("magnificationScale"), 1e100},
        {QStringLiteral("magnificationRadius"), -1e100}
    });

    DockConfigWatcher watcher(configPath(dir), componentsPath(dir));

    QCOMPARE(watcher.config().iconSize, 64);
    QCOMPARE(watcher.config().bottomMargin, 0);
    QCOMPARE(watcher.config().panelPadding, 21);
    QCOMPARE(watcher.config().magnificationScale, 2.0);
    QCOMPARE(watcher.config().magnificationRadius, 1.0);
}

void DockConfigWatcherTest::nonFiniteMagnificationFallsBack()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir().mkpath(QFileInfo(configPath(dir)).absolutePath()));
    QFile file(configPath(dir));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"({"magnificationScale":1e99999,"magnificationRadius":-1e99999})");
    file.close();

    DockConfigWatcher watcher(configPath(dir), componentsPath(dir));

    QCOMPARE(watcher.config().magnificationScale, 1.6);
    QCOMPARE(watcher.config().magnificationRadius, 2.5);
    QVERIFY(!watcher.lastError().isEmpty());
}

void DockConfigWatcherTest::invalidPinsAndDuplicatesAreRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeJson(configPath(dir), QJsonObject{
        {QStringLiteral("pins"), QJsonArray{
            QStringLiteral("one.desktop"), QStringLiteral("one.desktop"),
            QStringLiteral("../bad.desktop"), QStringLiteral("dir/bad.desktop"),
            QStringLiteral("two.desktop")}}
    });
    DockConfigWatcher watcher(configPath(dir), componentsPath(dir));
    const QStringList expected{QStringLiteral("one.desktop"), QStringLiteral("two.desktop")};
    QCOMPARE(watcher.config().pins, expected);
}

void DockConfigWatcherTest::oversizedInputUsesDefaults()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir().mkpath(QFileInfo(configPath(dir)).absolutePath()));
    QFile file(configPath(dir));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray(2 * 1024 * 1024, 'x'));
    file.close();
    DockConfigWatcher watcher(configPath(dir), componentsPath(dir));
    QCOMPARE(watcher.config().iconSize, 48);
    QVERIFY(!watcher.lastError().isEmpty());
}

void DockConfigWatcherTest::atomicReplacementReloadsAfterDebounce()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeJson(configPath(dir), QJsonObject{{QStringLiteral("iconSize"), 40}});
    DockConfigWatcher watcher(configPath(dir), componentsPath(dir));
    QSignalSpy changedSpy(&watcher, &DockConfigWatcher::configChanged);
    writeJson(configPath(dir), QJsonObject{{QStringLiteral("iconSize"), 52}});
    QTRY_COMPARE_WITH_TIMEOUT(watcher.config().iconSize, 52, 1500);
    QVERIFY(changedSpy.count() >= 1);
}

void DockConfigWatcherTest::componentToggleAndWatcherRecovery()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DockConfigWatcher watcher(configPath(dir), componentsPath(dir));
    writeJson(componentsPath(dir), QJsonObject{{QStringLiteral("dock"), false}});
    QTRY_VERIFY_WITH_TIMEOUT(!watcher.componentEnabled(), 1500);

    QVERIFY(QFile::remove(componentsPath(dir)));
    writeJson(componentsPath(dir), QJsonObject{{QStringLiteral("dock"), true}});
    QTRY_VERIFY_WITH_TIMEOUT(watcher.componentEnabled(), 1500);
}

QTEST_MAIN(DockConfigWatcherTest)
#include "DockConfigWatcherTest.moc"
