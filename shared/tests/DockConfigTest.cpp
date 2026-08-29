#include "dock/DockConfig.hpp"
#include "dock/DockConfigStore.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QTemporaryDir>

#include <limits>

class DockConfigTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsMatchThePersonalizationContract();
    void parsesNewFieldsAndLegacyEdgeMargin();
    void invalidEnumsFallbackIndependently();
    void nonFiniteNumbersFallbackAndFiniteValuesClamp();
    void storePreservesUnknownKeysAndExactPins();
    void personalizationWritePreservesRawPins();
    void malformedExistingJsonIsRefusedWithoutReplacement();
};

static QString configPath(const QTemporaryDir &directory)
{
    return directory.path() + QStringLiteral("/config/AstreaOS/dock.json");
}

static void writeObject(const QString &path, const QJsonObject &object)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    QCOMPARE(file.write(bytes), bytes.size());
}

static QJsonObject readObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

void DockConfigTest::defaultsMatchThePersonalizationContract()
{
    const DockConfig config = DockConfig::defaults();
    QCOMPARE(config.iconSize, 48);
    QCOMPARE(config.panelPadding, 14);
    QCOMPARE(config.itemSpacing, 10);
    QCOMPARE(config.hoverEffect, QStringLiteral("magnification"));
    QCOMPARE(config.magnificationScale, 1.6);
    QCOMPARE(config.magnificationRadius, 2.5);
    QCOMPARE(config.edgeMargin, 12);
    QCOMPARE(config.position, QStringLiteral("bottom"));
    QVERIFY(config.floating);
    QCOMPARE(config.cornerRadius, 23);
    QCOMPARE(config.autoHide, QStringLiteral("never"));
    QCOMPARE(config.indicatorStyle, QStringLiteral("line"));
    QCOMPARE(config.indicatorSize, 3);
    QVERIFY(config.animationsEnabled);
    QCOMPARE(config.animationSpeed, 1.0);
    QVERIFY(config.pins.isEmpty());
}

void DockConfigTest::parsesNewFieldsAndLegacyEdgeMargin()
{
    const QJsonObject object{
        {QStringLiteral("iconSize"), 56},
        {QStringLiteral("edgeMargin"), 31},
        {QStringLiteral("position"), QStringLiteral("right")},
        {QStringLiteral("floating"), false},
        {QStringLiteral("cornerRadius"), 17},
        {QStringLiteral("autoHide"), QStringLiteral("intelligent")},
        {QStringLiteral("indicatorStyle"), QStringLiteral("dot")},
        {QStringLiteral("indicatorSize"), 6},
        {QStringLiteral("animationsEnabled"), false},
        {QStringLiteral("animationSpeed"), 1.75},
        {QStringLiteral("pins"), QJsonArray{QStringLiteral("one.desktop")}}
    };
    QStringList errors;
    const DockConfig config = DockConfigCodec::parse(object, &errors);
    QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QStringLiteral("; "))));
    QCOMPARE(config.iconSize, 56);
    QCOMPARE(config.edgeMargin, 31);
    QCOMPARE(config.position, QStringLiteral("right"));
    QVERIFY(!config.floating);
    QCOMPARE(config.cornerRadius, 17);
    QCOMPARE(config.autoHide, QStringLiteral("intelligent"));
    QCOMPARE(config.indicatorStyle, QStringLiteral("dot"));
    QCOMPARE(config.indicatorSize, 6);
    QVERIFY(!config.animationsEnabled);
    QCOMPARE(config.animationSpeed, 1.75);

    errors.clear();
    const DockConfig legacy = DockConfigCodec::parse(
        QJsonObject{{QStringLiteral("bottomMargin"), 27}}, &errors);
    QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QStringLiteral("; "))));
    QCOMPARE(legacy.edgeMargin, 27);
}

void DockConfigTest::invalidEnumsFallbackIndependently()
{
    QStringList errors;
    const DockConfig config = DockConfigCodec::parse(
        QJsonObject{
            {QStringLiteral("hoverEffect"), QStringLiteral("invalid")},
            {QStringLiteral("position"), QStringLiteral("top")},
            {QStringLiteral("autoHide"), QStringLiteral("sometimes")},
            {QStringLiteral("indicatorStyle"), QStringLiteral("bar")}},
        &errors);

    QCOMPARE(config.hoverEffect, QStringLiteral("magnification"));
    QCOMPARE(config.position, QStringLiteral("bottom"));
    QCOMPARE(config.autoHide, QStringLiteral("never"));
    QCOMPARE(config.indicatorStyle, QStringLiteral("line"));
    QCOMPARE(errors.size(), 4);
}

void DockConfigTest::nonFiniteNumbersFallbackAndFiniteValuesClamp()
{
    QStringList errors;
    const DockConfig config = DockConfigCodec::parse(
        QJsonObject{
            {QStringLiteral("iconSize"), 1000},
            {QStringLiteral("cornerRadius"), -4},
            {QStringLiteral("indicatorSize"), 100},
            {QStringLiteral("animationSpeed"), 100.0},
            {QStringLiteral("magnificationScale"),
             QJsonValue::fromVariant(std::numeric_limits<double>::infinity())}},
        &errors);

    QCOMPARE(config.iconSize, 64);
    QCOMPARE(config.cornerRadius, 0);
    QCOMPARE(config.indicatorSize, 12);
    QCOMPARE(config.animationSpeed, 4.0);
    QCOMPARE(config.magnificationScale, 1.6);
    QVERIFY(!errors.isEmpty());
}

void DockConfigTest::storePreservesUnknownKeysAndExactPins()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    writeObject(path, QJsonObject{
        {QStringLiteral("futureFeature"), QJsonObject{{QStringLiteral("enabled"), true}}},
        {QStringLiteral("futureList"), QJsonArray{1, 2, 3}},
        {QStringLiteral("bottomMargin"), 22},
        {QStringLiteral("pins"), QJsonArray{QStringLiteral("first.desktop"),
                                             QStringLiteral("second.desktop")}}
    });

    DockConfig config = DockConfig::defaults();
    config.edgeMargin = 34;
    config.position = QStringLiteral("left");
    config.pins = {QStringLiteral("second.desktop"), QStringLiteral("first.desktop")};
    DockConfigStore store(path);
    QString error;
    QVERIFY2(store.writeConfig(config, &error), qPrintable(error));

    const QJsonObject result = readObject(path);
    QCOMPARE(result.value(QStringLiteral("futureFeature")).toObject().value(
                 QStringLiteral("enabled")).toBool(), true);
    const QJsonArray expectedFutureList{1, 2, 3};
    QCOMPARE(result.value(QStringLiteral("futureList")).toArray(), expectedFutureList);
    QVERIFY(!result.contains(QStringLiteral("bottomMargin")));
    QCOMPARE(result.value(QStringLiteral("edgeMargin")).toInt(), 34);
    QCOMPARE(result.value(QStringLiteral("position")).toString(), QStringLiteral("left"));
    const QJsonArray expectedPins{QStringLiteral("second.desktop"), QStringLiteral("first.desktop")};
    QCOMPARE(result.value(QStringLiteral("pins")).toArray(), expectedPins);
}

void DockConfigTest::personalizationWritePreservesRawPins()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    const QJsonArray rawPins{QStringLiteral("one.desktop"), QStringLiteral("one.desktop"),
                             QStringLiteral("not-a-pin")};
    writeObject(path, QJsonObject{
        {QStringLiteral("futureFeature"), true},
        {QStringLiteral("pins"), rawPins}
    });

    DockConfig config = DockConfigCodec::parse(readObject(path));
    config.iconSize = 52;
    DockConfigStore store(path);
    QString error;
    QVERIFY2(store.writePersonalization(config, &error), qPrintable(error));

    const QJsonObject result = readObject(path);
    QCOMPARE(result.value(QStringLiteral("futureFeature")).toBool(), true);
    QCOMPARE(result.value(QStringLiteral("pins")).toArray(), rawPins);
    QCOMPARE(result.value(QStringLiteral("iconSize")).toInt(), 52);
}

void DockConfigTest::malformedExistingJsonIsRefusedWithoutReplacement()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    const QByteArray original = QByteArrayLiteral("{not-json");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QCOMPARE(file.write(original), original.size());
    file.close();

    DockConfigStore store(path);
    QString error;
    QVERIFY(!store.writeConfig(DockConfig::defaults(), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(error.size() <= 512);

    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(file.readAll(), original);
}

QTEST_GUILESS_MAIN(DockConfigTest)
#include "DockConfigTest.moc"
