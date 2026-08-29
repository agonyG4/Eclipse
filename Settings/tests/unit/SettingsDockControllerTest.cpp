#include "services/dock/SettingsDockController.hpp"

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

class SettingsDockControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void readsDefaultsFromInjectedPath();
    void typedSettersPersistCanonicalConfig();
    void invalidValuesAreClampedOrRejected();
    void unchangedValueDoesNotEmitPropertyChange();
    void externalReplacementUpdatesProperties();
    void failedWriteRestoresPreviousStateAndReportsBoundedError();
};

static QString configPath(const QTemporaryDir &directory)
{
    return directory.path() + QStringLiteral("/config/AstreaOS/dock.json");
}

static void writeObject(const QString &path, const QJsonObject &object)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QSaveFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    QCOMPARE(file.write(bytes), bytes.size());
    QVERIFY(file.commit());
}

static QJsonObject readObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

void SettingsDockControllerTest::readsDefaultsFromInjectedPath()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsDockController controller(configPath(directory));

    QCOMPARE(controller.iconSize(), 48);
    QCOMPARE(controller.panelPadding(), 14);
    QCOMPARE(controller.itemSpacing(), 10);
    QCOMPARE(controller.hoverEffect(), QStringLiteral("magnification"));
    QCOMPARE(controller.edgeMargin(), 12);
    QCOMPARE(controller.position(), QStringLiteral("bottom"));
    QVERIFY(controller.floating());
    QCOMPARE(controller.cornerRadius(), 23);
    QCOMPARE(controller.autoHide(), QStringLiteral("never"));
    QCOMPARE(controller.indicatorStyle(), QStringLiteral("line"));
    QCOMPARE(controller.indicatorSize(), 3);
    QVERIFY(controller.animationsEnabled());
    QCOMPARE(controller.animationSpeed(), 1.0);
    QVERIFY(controller.lastError().isEmpty());
}

void SettingsDockControllerTest::typedSettersPersistCanonicalConfig()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    writeObject(path, QJsonObject{
        {QStringLiteral("future"), QStringLiteral("preserve")},
        {QStringLiteral("pins"), QJsonArray{QStringLiteral("one.desktop")}}
    });
    SettingsDockController controller(path);

    controller.setIconSize(60);
    controller.setPosition(QStringLiteral("left"));
    controller.setFloating(false);
    controller.setAnimationSpeed(2.5);
    controller.flush();

    const QJsonObject object = readObject(path);
    QCOMPARE(object.value(QStringLiteral("future")).toString(), QStringLiteral("preserve"));
    QCOMPARE(object.value(QStringLiteral("pins")).toArray(),
             QJsonArray{QStringLiteral("one.desktop")});
    QCOMPARE(object.value(QStringLiteral("iconSize")).toInt(), 60);
    QCOMPARE(object.value(QStringLiteral("position")).toString(), QStringLiteral("left"));
    QCOMPARE(object.value(QStringLiteral("floating")).toBool(), false);
    QCOMPARE(object.value(QStringLiteral("edgeMargin")).toInt(), 12);
    QVERIFY(!object.contains(QStringLiteral("bottomMargin")));
    QCOMPARE(object.value(QStringLiteral("animationSpeed")).toDouble(), 2.5);
}

void SettingsDockControllerTest::invalidValuesAreClampedOrRejected()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsDockController controller(configPath(directory));

    controller.setIconSize(1000);
    controller.setPanelPadding(-1);
    controller.setMagnificationScale(100.0);
    controller.setAnimationSpeed(0.0);
    controller.setPosition(QStringLiteral("top"));
    controller.setHoverEffect(QStringLiteral("unsupported"));

    QCOMPARE(controller.iconSize(), 64);
    QCOMPARE(controller.panelPadding(), 8);
    QCOMPARE(controller.magnificationScale(), 2.0);
    QCOMPARE(controller.animationSpeed(), 0.25);
    QCOMPARE(controller.position(), QStringLiteral("bottom"));
    QCOMPARE(controller.hoverEffect(), QStringLiteral("magnification"));
    controller.flush();
    const QJsonObject object = readObject(configPath(directory));
    QCOMPARE(object.value(QStringLiteral("position")).toString(), QStringLiteral("bottom"));
    QCOMPARE(object.value(QStringLiteral("hoverEffect")).toString(),
             QStringLiteral("magnification"));
}

void SettingsDockControllerTest::unchangedValueDoesNotEmitPropertyChange()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsDockController controller(configPath(directory));
    QSignalSpy spy(&controller, &SettingsDockController::iconSizeChanged);

    controller.setIconSize(48);
    QCOMPARE(spy.count(), 0);
}

void SettingsDockControllerTest::externalReplacementUpdatesProperties()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    SettingsDockController controller(path);
    writeObject(path, QJsonObject{
        {QStringLiteral("iconSize"), 40},
        {QStringLiteral("position"), QStringLiteral("right")},
        {QStringLiteral("autoHide"), QStringLiteral("always")}
    });

    QTRY_COMPARE_WITH_TIMEOUT(controller.iconSize(), 40, 1500);
    QTRY_COMPARE_WITH_TIMEOUT(controller.position(), QStringLiteral("right"), 1500);
    QTRY_COMPARE_WITH_TIMEOUT(controller.autoHide(), QStringLiteral("always"), 1500);
}

void SettingsDockControllerTest::failedWriteRestoresPreviousStateAndReportsBoundedError()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString parentPath = directory.path() + QStringLiteral("/not-a-directory");
    QFile parent(parentPath);
    QVERIFY(parent.open(QIODevice::WriteOnly));
    parent.write("file");
    parent.close();

    SettingsDockController controller(parentPath + QStringLiteral("/dock.json"));
    controller.setIconSize(60);
    controller.flush();

    QCOMPARE(controller.iconSize(), 48);
    QVERIFY(!controller.lastError().isEmpty());
    QVERIFY(controller.lastError().size() <= 512);
}

QTEST_GUILESS_MAIN(SettingsDockControllerTest)
#include "SettingsDockControllerTest.moc"
