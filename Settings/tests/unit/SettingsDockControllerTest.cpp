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
    void defaultPropertiesMatchSharedConfig();
    void isDefaultTracksPersonalizationOnly();
    void typedSettersPersistCanonicalConfig();
    void invalidValuesAreClampedOrRejected();
    void unchangedValueDoesNotEmitPropertyChange();
    void externalReplacementUpdatesProperties();
    void pendingWriteDefersExternalRefreshUntilFlush();
    void flushPreservesExternalPinsWhileApplyingLocalDraft();
    void failedWriteRestoresPreviousStateAndReportsBoundedError();
    void restoreDefaultsPreservesPinsAndUnknownKeys();
    void restoreDefaultsCapturesUnflushedDraftAndUndoRestoresIt();
    void undoPreservesPinsChangedExternally();
    void failedRestoreDoesNotOfferUndo();
    void undoRestoreExpiresOnce();
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

static void comparePersonalization(const SettingsDockController &controller,
                                   const DockConfig &expected)
{
    QCOMPARE(controller.iconSize(), expected.iconSize);
    QCOMPARE(controller.panelPadding(), expected.panelPadding);
    QCOMPARE(controller.itemSpacing(), expected.itemSpacing);
    QCOMPARE(controller.hoverEffect(), expected.hoverEffect);
    QCOMPARE(controller.magnificationScale(), expected.magnificationScale);
    QCOMPARE(controller.magnificationRadius(), expected.magnificationRadius);
    QCOMPARE(controller.edgeMargin(), expected.edgeMargin);
    QCOMPARE(controller.position(), expected.position);
    QCOMPARE(controller.floating(), expected.floating);
    QCOMPARE(controller.cornerRadius(), expected.cornerRadius);
    QCOMPARE(controller.autoHide(), expected.autoHide);
    QCOMPARE(controller.indicatorStyle(), expected.indicatorStyle);
    QCOMPARE(controller.indicatorSize(), expected.indicatorSize);
    QCOMPARE(controller.animationsEnabled(), expected.animationsEnabled);
    QCOMPARE(controller.animationSpeed(), expected.animationSpeed);
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

void SettingsDockControllerTest::defaultPropertiesMatchSharedConfig()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsDockController controller(configPath(directory));
    const DockConfig defaults = DockConfig::defaults();

    QCOMPARE(controller.property("defaultIconSize").toInt(), defaults.iconSize);
    QCOMPARE(controller.property("defaultPanelPadding").toInt(), defaults.panelPadding);
    QCOMPARE(controller.property("defaultItemSpacing").toInt(), defaults.itemSpacing);
    QCOMPARE(controller.property("defaultMagnificationScale").toDouble(),
             defaults.magnificationScale);
    QCOMPARE(controller.property("defaultMagnificationRadius").toDouble(),
             defaults.magnificationRadius);
    QCOMPARE(controller.property("defaultEdgeMargin").toInt(), defaults.edgeMargin);
    QCOMPARE(controller.property("defaultCornerRadius").toInt(), defaults.cornerRadius);
    QCOMPARE(controller.property("defaultIndicatorSize").toInt(), defaults.indicatorSize);
    QCOMPARE(controller.property("defaultAnimationSpeed").toDouble(), defaults.animationSpeed);
}

void SettingsDockControllerTest::isDefaultTracksPersonalizationOnly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    SettingsDockController controller(path);
    const DockConfig defaults = DockConfig::defaults();
    QSignalSpy spy(&controller, SIGNAL(isDefaultChanged()));
    QVERIFY(spy.isValid());

    QVERIFY(controller.property("isDefault").toBool());
    controller.setIconSize(defaults.iconSize + 1);
    QVERIFY(!controller.property("isDefault").toBool());
    QCOMPARE(spy.count(), 1);

    controller.setIconSize(defaults.iconSize + 1);
    QCOMPARE(spy.count(), 1);
    controller.setIconSize(defaults.iconSize);
    QVERIFY(controller.property("isDefault").toBool());
    QCOMPARE(spy.count(), 2);
    controller.flush();

    writeObject(path, QJsonObject{{QStringLiteral("pins"),
                                   QJsonArray{QStringLiteral("only-pins.desktop")}}});
    controller.refresh();
    QVERIFY(controller.property("isDefault").toBool());
    QCOMPARE(spy.count(), 2);
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

void SettingsDockControllerTest::pendingWriteDefersExternalRefreshUntilFlush()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    writeObject(path, QJsonObject{{QStringLiteral("iconSize"), 40}});
    SettingsDockController controller(path);

    controller.setIconSize(60);
    writeObject(path, QJsonObject{{QStringLiteral("iconSize"), 40},
                                  {QStringLiteral("position"), QStringLiteral("right")}});
    QTest::qWait(130);

    QCOMPARE(controller.iconSize(), 60);
    QCOMPARE(controller.position(), QStringLiteral("bottom"));
    QVERIFY(controller.pendingWrite());
}

void SettingsDockControllerTest::flushPreservesExternalPinsWhileApplyingLocalDraft()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    writeObject(path, QJsonObject{{QStringLiteral("iconSize"), 40},
                                  {QStringLiteral("pins"), QJsonArray{QStringLiteral("old.desktop")}}});
    SettingsDockController controller(path);

    controller.setIconSize(60);
    writeObject(path, QJsonObject{{QStringLiteral("iconSize"), 40},
                                  {QStringLiteral("pins"), QJsonArray{QStringLiteral("new.desktop")}},
                                  {QStringLiteral("future"), QStringLiteral("preserve")}});
    QTest::qWait(130);
    QCOMPARE(controller.iconSize(), 60);

    controller.flush();
    const QJsonObject object = readObject(path);
    QCOMPARE(object.value(QStringLiteral("iconSize")).toInt(), 60);
    QCOMPARE(object.value(QStringLiteral("pins")).toArray(),
             QJsonArray{QStringLiteral("new.desktop")});
    QCOMPARE(object.value(QStringLiteral("future")).toString(), QStringLiteral("preserve"));
    QCOMPARE(controller.iconSize(), 60);
    QVERIFY(!controller.pendingWrite());
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

void SettingsDockControllerTest::restoreDefaultsPreservesPinsAndUnknownKeys()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    writeObject(path, QJsonObject{
        {QStringLiteral("iconSize"), 60},
        {QStringLiteral("panelPadding"), 24},
        {QStringLiteral("hoverEffect"), QStringLiteral("none")},
        {QStringLiteral("magnificationScale"), 1.9},
        {QStringLiteral("position"), QStringLiteral("left")},
        {QStringLiteral("floating"), false},
        {QStringLiteral("animationSpeed"), 2.5},
        {QStringLiteral("pins"), QJsonArray{QStringLiteral("old.desktop")}},
        {QStringLiteral("futureField"), QJsonObject{{QStringLiteral("enabled"), true}}}
    });
    SettingsDockController controller(path);
    const DockConfig defaults = DockConfig::defaults();
    QVERIFY(!controller.property("isDefault").toBool());

    QVERIFY(QMetaObject::invokeMethod(&controller, "restoreDefaults"));
    comparePersonalization(controller, defaults);
    QVERIFY(controller.property("isDefault").toBool());
    QVERIFY(!controller.pendingWrite());
    QVERIFY(controller.property("canUndoRestore").toBool());

    const QJsonObject object = readObject(path);
    const DockConfig persisted = DockConfigCodec::parse(object);
    comparePersonalization(controller, persisted);
    comparePersonalization(controller, defaults);
    QCOMPARE(object.value(QStringLiteral("pins")).toArray(),
             QJsonArray{QStringLiteral("old.desktop")});
    const QJsonObject expectedFuture{{QStringLiteral("enabled"), true}};
    QCOMPARE(object.value(QStringLiteral("futureField")).toObject(), expectedFuture);
}

void SettingsDockControllerTest::restoreDefaultsCapturesUnflushedDraftAndUndoRestoresIt()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    SettingsDockController controller(path);
    controller.setIconSize(60);
    controller.setPanelPadding(22);
    controller.setHoverEffect(QStringLiteral("lift"));
    controller.setMagnificationScale(1.8);
    controller.setPosition(QStringLiteral("right"));
    controller.setFloating(false);
    controller.setAnimationSpeed(2.0);
    QVERIFY(controller.pendingWrite());

    QVERIFY(QMetaObject::invokeMethod(&controller, "restoreDefaults"));
    QVERIFY(controller.property("canUndoRestore").toBool());
    comparePersonalization(controller, DockConfig::defaults());

    QVERIFY(QMetaObject::invokeMethod(&controller, "undoRestore"));
    QCOMPARE(controller.iconSize(), 60);
    QCOMPARE(controller.panelPadding(), 22);
    QCOMPARE(controller.hoverEffect(), QStringLiteral("lift"));
    QCOMPARE(controller.magnificationScale(), 1.8);
    QCOMPARE(controller.position(), QStringLiteral("right"));
    QCOMPARE(controller.floating(), false);
    QCOMPARE(controller.animationSpeed(), 2.0);
    QVERIFY(!controller.property("canUndoRestore").toBool());
    QVERIFY(!controller.pendingWrite());

    const QJsonObject object = readObject(path);
    QCOMPARE(object.value(QStringLiteral("iconSize")).toInt(), 60);
    QCOMPARE(object.value(QStringLiteral("panelPadding")).toInt(), 22);
    QCOMPARE(object.value(QStringLiteral("hoverEffect")).toString(), QStringLiteral("lift"));
    QCOMPARE(object.value(QStringLiteral("position")).toString(), QStringLiteral("right"));
}

void SettingsDockControllerTest::undoPreservesPinsChangedExternally()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    writeObject(path, QJsonObject{
        {QStringLiteral("iconSize"), 60},
        {QStringLiteral("pins"), QJsonArray{QStringLiteral("old.desktop")}},
        {QStringLiteral("futureField"), QStringLiteral("preserve")}
    });
    SettingsDockController controller(path);
    QVERIFY(QMetaObject::invokeMethod(&controller, "restoreDefaults"));
    QVERIFY(controller.property("canUndoRestore").toBool());

    writeObject(path, QJsonObject{
        {QStringLiteral("iconSize"), DockConfig::defaults().iconSize},
        {QStringLiteral("pins"), QJsonArray{QStringLiteral("new.desktop")}},
        {QStringLiteral("futureField"), QStringLiteral("preserve")}
    });
    controller.refresh();
    QVERIFY(QMetaObject::invokeMethod(&controller, "undoRestore"));

    QCOMPARE(controller.iconSize(), 60);
    const QJsonObject object = readObject(path);
    QCOMPARE(object.value(QStringLiteral("pins")).toArray(),
             QJsonArray{QStringLiteral("new.desktop")});
    QCOMPARE(object.value(QStringLiteral("futureField")).toString(), QStringLiteral("preserve"));
    QVERIFY(!controller.property("canUndoRestore").toBool());
}

void SettingsDockControllerTest::failedRestoreDoesNotOfferUndo()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    writeObject(path, QJsonObject{{QStringLiteral("iconSize"), 60}});
    SettingsDockController controller(path);
    const QString configDirectory = QFileInfo(path).absolutePath();
    QVERIFY(QFile::setPermissions(configDirectory,
                                  QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    QVERIFY(QMetaObject::invokeMethod(&controller, "restoreDefaults"));
    QVERIFY(QFile::setPermissions(configDirectory,
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner));

    QCOMPARE(controller.iconSize(), 60);
    QVERIFY(!controller.property("canUndoRestore").toBool());
    QVERIFY(!controller.pendingWrite());
    QVERIFY(!controller.lastError().isEmpty());
}

void SettingsDockControllerTest::undoRestoreExpiresOnce()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = configPath(directory);
    writeObject(path, QJsonObject{{QStringLiteral("iconSize"), 60}});
    SettingsDockController controller(path);
    QSignalSpy spy(&controller, SIGNAL(canUndoRestoreChanged()));
    QVERIFY(spy.isValid());

    QVERIFY(QMetaObject::invokeMethod(&controller, "restoreDefaults"));
    QVERIFY(controller.property("canUndoRestore").toBool());
    QCOMPARE(spy.count(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.property("canUndoRestore").toBool(), 5200);
    QCOMPARE(spy.count(), 2);
}

QTEST_GUILESS_MAIN(SettingsDockControllerTest)
#include "SettingsDockControllerTest.moc"
