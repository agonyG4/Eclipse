#include "screenshot/ScreenshotConfiguration.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTest>
#include <QTemporaryDir>

class ScreenshotConfigurationTest final : public QObject {
    Q_OBJECT

private slots:
    void usesPredictableDefaults();
    void normalizesInvalidFieldsIndependently();
    void settersBoundNumericValuesAndRejectUnknownEnums();
    void atomicEditsPreserveUnrelatedKeys();
    void editingNormalizesOtherInvalidKnownFields();
    void resetWritesAllDefaultsAndPreservesUnknownKeys();
    void malformedAndOversizedFilesFallBackSafely();
    void externalAtomicReplacementReloads();
    void rapidEditsKeepTheNewestValue();
};

namespace {

static void writeBytes(const QString &path, const QByteArray &bytes)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QSaveFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(bytes), bytes.size());
    QVERIFY(file.commit());
}

static QJsonObject readObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

static QString configPath(const QTemporaryDir &directory)
{
    return directory.filePath(QStringLiteral("config/AstreaOS/screenshot.json"));
}

} // namespace

void ScreenshotConfigurationTest::usesPredictableDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ScreenshotConfiguration configuration(configPath(directory));

    QCOMPARE(configuration.cornerRadius(), 10);
    QVERIFY(configuration.borderEnabled());
    QCOMPARE(configuration.borderWidth(), 1);
    QVERIFY(configuration.borderColor().isValid());
    QVERIFY(configuration.shadowEnabled());
    QCOMPARE(configuration.shadowStrength(), 0.55);
    QCOMPARE(configuration.thumbnailWidth(), 240);
    QCOMPARE(configuration.thumbnailHeight(), 160);
    QCOMPARE(configuration.previewMaximumScale(), 0.78);
    QCOMPARE(configuration.position(), QStringLiteral("bottom-right"));
    QCOMPARE(configuration.edgeMargin(), 20);
    QVERIFY(configuration.animationsEnabled());
    QCOMPARE(configuration.entryAnimation(), QStringLiteral("scale-fade"));
    QCOMPARE(configuration.exitAnimation(), QStringLiteral("slide-fade"));
    QCOMPARE(configuration.entryDurationMs(), 160);
    QCOMPARE(configuration.exitDurationMs(), 220);
    QCOMPARE(configuration.entryEasing(), QStringLiteral("out-cubic"));
    QCOMPARE(configuration.exitEasing(), QStringLiteral("out-cubic"));
    QVERIFY(configuration.showThumbnail());
    QVERIFY(configuration.autoCopy());
    QVERIFY(configuration.dragEnabled());
    QCOMPARE(configuration.displayDurationMs(), 5000);
    QCOMPARE(configuration.clickAction(), QStringLiteral("preview"));
    QCOMPARE(configuration.status(), QStringLiteral("ready"));
}

void ScreenshotConfigurationTest::normalizesInvalidFieldsIndependently()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    const QJsonObject invalid{
        {QStringLiteral("cornerRadius"), 1000},
        {QStringLiteral("borderEnabled"), QStringLiteral("yes")},
        {QStringLiteral("position"), QStringLiteral("center")},
        {QStringLiteral("thumbnailSize"), QJsonObject{
             {QStringLiteral("width"), 1},
             {QStringLiteral("height"), 120}}},
        {QStringLiteral("displayDurationMs"), -12},
        {QStringLiteral("clickAction"), QStringLiteral("delete")},
    };
    writeBytes(path, QJsonDocument(invalid).toJson(QJsonDocument::Compact));

    ScreenshotConfiguration configuration(path);

    QCOMPARE(configuration.cornerRadius(), 10);
    QVERIFY(configuration.borderEnabled());
    QCOMPARE(configuration.position(), QStringLiteral("bottom-right"));
    QCOMPARE(configuration.thumbnailWidth(), 240);
    QCOMPARE(configuration.thumbnailHeight(), 120);
    QCOMPARE(configuration.displayDurationMs(), 5000);
    QCOMPARE(configuration.clickAction(), QStringLiteral("preview"));
    QCOMPARE(configuration.status(), QStringLiteral("error"));
}

void ScreenshotConfigurationTest::settersBoundNumericValuesAndRejectUnknownEnums()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ScreenshotConfiguration configuration(configPath(directory));

    configuration.setCornerRadius(90);
    QCOMPARE(configuration.cornerRadius(), 64);
    configuration.setBorderWidth(-5);
    QCOMPARE(configuration.borderWidth(), 0);
    configuration.setShadowStrength(4.0);
    QCOMPARE(configuration.shadowStrength(), 1.0);
    configuration.setEdgeMargin(500);
    QCOMPARE(configuration.edgeMargin(), 120);
    configuration.setPosition(QStringLiteral("center"));
    QCOMPARE(configuration.position(), QStringLiteral("bottom-right"));
    configuration.setClickAction(QStringLiteral("delete"));
    QCOMPARE(configuration.clickAction(), QStringLiteral("preview"));
}

void ScreenshotConfigurationTest::atomicEditsPreserveUnrelatedKeys()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    const QJsonObject original{
        {QStringLiteral("vendorExtension"), QJsonObject{{QStringLiteral("enabled"), true}}},
        {QStringLiteral("thumbnailSize"), QJsonObject{
             {QStringLiteral("width"), 220},
             {QStringLiteral("height"), 150},
             {QStringLiteral("vendorField"), QStringLiteral("keep")}}},
    };
    writeBytes(path, QJsonDocument(original).toJson(QJsonDocument::Compact));
    ScreenshotConfiguration configuration(path);

    configuration.setThumbnailWidth(300);

    const QJsonObject saved = readObject(path);
    QCOMPARE(saved.value(QStringLiteral("vendorExtension")).toObject()
                 .value(QStringLiteral("enabled")).toBool(), true);
    const QJsonObject size = saved.value(QStringLiteral("thumbnailSize")).toObject();
    QCOMPARE(size.value(QStringLiteral("width")).toInt(), 300);
    QCOMPARE(size.value(QStringLiteral("height")).toInt(), 150);
    QCOMPARE(size.value(QStringLiteral("vendorField")).toString(), QStringLiteral("keep"));
}

void ScreenshotConfigurationTest::editingNormalizesOtherInvalidKnownFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    writeBytes(path, QJsonDocument(QJsonObject{
        {QStringLiteral("extension"), QStringLiteral("keep")},
        {QStringLiteral("cornerRadius"), 1000},
        {QStringLiteral("position"), QStringLiteral("center")},
    }).toJson(QJsonDocument::Compact));
    ScreenshotConfiguration configuration(path);

    configuration.setEdgeMargin(32);

    const QJsonObject saved = readObject(path);
    QCOMPARE(saved.value(QStringLiteral("extension")).toString(), QStringLiteral("keep"));
    QCOMPARE(saved.value(QStringLiteral("cornerRadius")).toInt(), 10);
    QCOMPARE(saved.value(QStringLiteral("position")).toString(), QStringLiteral("bottom-right"));
    QCOMPARE(saved.value(QStringLiteral("edgeMargin")).toInt(), 32);
}

void ScreenshotConfigurationTest::resetWritesAllDefaultsAndPreservesUnknownKeys()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    writeBytes(path, QJsonDocument(QJsonObject{
        {QStringLiteral("extension"), QStringLiteral("keep")},
        {QStringLiteral("cornerRadius"), 18},
    }).toJson(QJsonDocument::Compact));
    ScreenshotConfiguration configuration(path);
    configuration.setCornerRadius(28);

    configuration.resetToDefaults();

    QCOMPARE(configuration.cornerRadius(), 10);
    QCOMPARE(configuration.position(), QStringLiteral("bottom-right"));
    const QJsonObject saved = readObject(path);
    QCOMPARE(saved.value(QStringLiteral("extension")).toString(), QStringLiteral("keep"));
    QCOMPARE(saved.value(QStringLiteral("cornerRadius")).toInt(), 10);
    QVERIFY(saved.value(QStringLiteral("borderEnabled")).toBool());
    QVERIFY(saved.value(QStringLiteral("thumbnailSize")).isObject());
    QCOMPARE(saved.value(QStringLiteral("displayDurationMs")).toInt(), 5000);
}

void ScreenshotConfigurationTest::malformedAndOversizedFilesFallBackSafely()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    writeBytes(path, QByteArrayLiteral("{broken"));
    ScreenshotConfiguration malformed(path);
    QCOMPARE(malformed.cornerRadius(), 10);
    QCOMPARE(malformed.status(), QStringLiteral("error"));

    writeBytes(path, QByteArray(64 * 1024 + 1, 'x'));
    ScreenshotConfiguration oversized(path);
    QCOMPARE(oversized.cornerRadius(), 10);
    QCOMPARE(oversized.status(), QStringLiteral("error"));
}

void ScreenshotConfigurationTest::externalAtomicReplacementReloads()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    ScreenshotConfiguration configuration(path);

    writeBytes(path, QJsonDocument(QJsonObject{
        {QStringLiteral("cornerRadius"), 32},
        {QStringLiteral("position"), QStringLiteral("top-left")},
    }).toJson(QJsonDocument::Compact));

    QTRY_COMPARE_WITH_TIMEOUT(configuration.cornerRadius(), 32, 1500);
    QCOMPARE(configuration.position(), QStringLiteral("top-left"));
}

void ScreenshotConfigurationTest::rapidEditsKeepTheNewestValue()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    ScreenshotConfiguration configuration(path);

    configuration.setCornerRadius(16);
    configuration.setCornerRadius(19);
    configuration.setCornerRadius(23);

    QCOMPARE(configuration.cornerRadius(), 23);
    QCOMPARE(readObject(path).value(QStringLiteral("cornerRadius")).toInt(), 23);
}

QTEST_GUILESS_MAIN(ScreenshotConfigurationTest)

#include "ScreenshotConfigurationTest.moc"
