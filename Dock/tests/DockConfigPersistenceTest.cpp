#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>

#include "services/DockConfigPersistence.hpp"
#include "services/DockConfigWatcher.hpp"

class DockConfigPersistenceTest final : public QObject {
    Q_OBJECT

private slots:
    void updatesOnlyPinsAndPreservesKnownFields();
    void updatesOnlyPinsAndPreservesUnknownFields();
    void missingFileCanBeCreated();
    void malformedExistingJsonIsNotDestroyed();
    void duplicateAndInvalidPinsAreRejected();
    void atomicReplacementSucceeds();
    void writeFailureReturnsBoundedError();
    void watcherReloadsAtomicallyReplacedPins();
};

static QString configPath(const QTemporaryDir &dir)
{
    return dir.path() + QStringLiteral("/config/AstreaOS/dock.json");
}

static void writeBytes(const QString &path, const QByteArray &bytes)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(bytes), bytes.size());
}

static void writeObject(const QString &path, const QJsonObject &object)
{
    writeBytes(path, QJsonDocument(object).toJson(QJsonDocument::Compact));
}

static QJsonObject readObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject{};
}

void DockConfigPersistenceTest::updatesOnlyPinsAndPreservesKnownFields()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = configPath(dir);
    writeObject(path, QJsonObject{
        {QStringLiteral("iconSize"), 56},
        {QStringLiteral("bottomMargin"), 20},
        {QStringLiteral("panelPadding"), 18},
        {QStringLiteral("itemSpacing"), 12},
        {QStringLiteral("hoverEffect"), QStringLiteral("lift")},
        {QStringLiteral("magnificationScale"), 1.25},
        {QStringLiteral("magnificationRadius"), 3.5},
        {QStringLiteral("pins"), QJsonArray{QStringLiteral("one.desktop")}}
    });

    DockConfigPersistence persistence(path);
    QString error;
    QVERIFY2(persistence.writePins({QStringLiteral("two.desktop")}, &error), qPrintable(error));

    const QJsonObject object = readObject(path);
    QCOMPARE(object.value(QStringLiteral("iconSize")).toInt(), 56);
    QCOMPARE(object.value(QStringLiteral("bottomMargin")).toInt(), 20);
    QCOMPARE(object.value(QStringLiteral("panelPadding")).toInt(), 18);
    QCOMPARE(object.value(QStringLiteral("itemSpacing")).toInt(), 12);
    QCOMPARE(object.value(QStringLiteral("hoverEffect")).toString(), QStringLiteral("lift"));
    QCOMPARE(object.value(QStringLiteral("magnificationScale")).toDouble(), 1.25);
    QCOMPARE(object.value(QStringLiteral("magnificationRadius")).toDouble(), 3.5);
    QCOMPARE(object.value(QStringLiteral("pins")).toArray(),
             QJsonArray{QStringLiteral("two.desktop")});
}

void DockConfigPersistenceTest::updatesOnlyPinsAndPreservesUnknownFields()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = configPath(dir);
    writeObject(path, QJsonObject{
        {QStringLiteral("futureFeature"), QJsonObject{{QStringLiteral("enabled"), true}}},
        {QStringLiteral("futureList"), QJsonArray{1, 2, 3}},
        {QStringLiteral("pins"), QJsonArray{QStringLiteral("one.desktop")}}
    });

    DockConfigPersistence persistence(path);
    QVERIFY(persistence.writePins({QStringLiteral("two.desktop"), QStringLiteral("three.desktop")}));

    const QJsonObject object = readObject(path);
    QCOMPARE(object.value(QStringLiteral("futureFeature")).toObject().value(
                 QStringLiteral("enabled")).toBool(), true);
    const QJsonArray expectedList{1, 2, 3};
    QCOMPARE(object.value(QStringLiteral("futureList")).toArray(), expectedList);
}

void DockConfigPersistenceTest::missingFileCanBeCreated()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = configPath(dir);
    DockConfigPersistence persistence(path);

    QString error;
    QVERIFY2(persistence.writePins({QStringLiteral("one.desktop")}, &error), qPrintable(error));
    QCOMPARE(readObject(path).value(QStringLiteral("pins")).toArray(),
             QJsonArray{QStringLiteral("one.desktop")});
}

void DockConfigPersistenceTest::malformedExistingJsonIsNotDestroyed()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = configPath(dir);
    const QByteArray original = "{not-json";
    writeBytes(path, original);

    DockConfigPersistence persistence(path);
    QString error;
    QVERIFY(!persistence.writePins({QStringLiteral("one.desktop")}, &error));
    QVERIFY(error.size() <= 512);
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), original);
}

void DockConfigPersistenceTest::duplicateAndInvalidPinsAreRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = configPath(dir);
    const QByteArray original = R"({"pins":["old.desktop"]})";
    writeBytes(path, original);
    DockConfigPersistence persistence(path);

    QString error;
    QVERIFY(!persistence.writePins({QStringLiteral("one.desktop"), QStringLiteral("one.desktop")}, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!persistence.writePins({QStringLiteral("../bad.desktop")}, &error));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), original);
}

void DockConfigPersistenceTest::atomicReplacementSucceeds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = configPath(dir);
    writeObject(path, QJsonObject{{QStringLiteral("pins"), QJsonArray{QStringLiteral("one.desktop")}}});
    DockConfigPersistence persistence(path);

    QVERIFY(persistence.writePins({QStringLiteral("two.desktop")}));
    QCOMPARE(readObject(path).value(QStringLiteral("pins")).toArray(),
             QJsonArray{QStringLiteral("two.desktop")});
}

void DockConfigPersistenceTest::writeFailureReturnsBoundedError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString parentPath = dir.path() + QStringLiteral("/not-a-directory");
    writeBytes(parentPath, QByteArrayLiteral("file"));
    DockConfigPersistence persistence(parentPath + QStringLiteral("/dock.json"));

    QString error;
    QVERIFY(!persistence.writePins({QStringLiteral("one.desktop")}, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(error.size() <= 512);
}

void DockConfigPersistenceTest::watcherReloadsAtomicallyReplacedPins()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = configPath(dir);
    const QString components = dir.path() + QStringLiteral("/config/AstreaOS/ui/components.json");
    writeObject(path, QJsonObject{{QStringLiteral("pins"), QJsonArray{QStringLiteral("one.desktop")}}});
    DockConfigWatcher watcher(path, components);
    DockConfigPersistence persistence(path);

    QVERIFY(persistence.writePins({QStringLiteral("two.desktop")}));
    QTRY_COMPARE_WITH_TIMEOUT(watcher.config().pins,
                              QStringList{QStringLiteral("two.desktop")}, 1500);
}

QTEST_MAIN(DockConfigPersistenceTest)
#include "DockConfigPersistenceTest.moc"
