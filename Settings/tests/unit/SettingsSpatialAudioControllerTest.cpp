#include "core/SettingsController.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace {

class XdgConfigHome final {
public:
    explicit XdgConfigHome(const QString &path)
        : m_wasSet(qEnvironmentVariableIsSet("XDG_CONFIG_HOME"))
        , m_previous(qgetenv("XDG_CONFIG_HOME"))
    {
        qputenv("XDG_CONFIG_HOME", path.toUtf8());
    }

    ~XdgConfigHome()
    {
        if (m_wasSet)
            qputenv("XDG_CONFIG_HOME", m_previous);
        else
            qunsetenv("XDG_CONFIG_HOME");
    }

private:
    bool m_wasSet = false;
    QByteArray m_previous;
};

QString configPath(const QString &configHome)
{
    return QDir(configHome).filePath(QStringLiteral("AstreaOS/audio/spatial.json"));
}

QObject *spatialAudioController(SettingsController &controller)
{
    return controller.property("spatialAudio").value<QObject *>();
}

bool invokeSetEnabled(QObject *controller, bool enabled, bool *invoked = nullptr)
{
    if (!controller) {
        if (invoked)
            *invoked = false;
        return false;
    }

    bool succeeded = false;
    const bool methodInvoked = QMetaObject::invokeMethod(
        controller, "setSpatialAudioEnabled", Q_RETURN_ARG(bool, succeeded),
        Q_ARG(bool, enabled));
    if (invoked)
        *invoked = methodInvoked;
    return succeeded;
}

bool writeOrdinary(const QString &path, const QByteArray &contents)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size();
}

bool writeAtomically(const QString &path, const QByteArray &contents)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size()
        && file.commit();
}

QJsonObject readObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

} // namespace

class SettingsSpatialAudioControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void missingConfigurationDefaultsDisabledAndUsesXdgPath();
    void missingEnabledKeyDefaultsDisabled();
    void enabledValuesAreLoaded_data();
    void enabledValuesAreLoaded();
    void malformedConfigurationIsSafelyDisabledWithDiagnostic();
    void malformedConfigurationCanBeReplacedByToggle();
    void toggleWritesBothStatesAndCreatesParentDirectory();
    void preservesUnknownConfigurationFields();
    void failedPersistenceKeepsLastPersistedState();
    void externalWritesAndAtomicReplacementUpdateState();
    void deletingConfigurationDisablesSpatialAudio();
    void equivalentFilesystemEventsDoNotRepeatSemanticChanges();
};

void SettingsSpatialAudioControllerTest::missingConfigurationDefaultsDisabledAndUsesXdgPath()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    const XdgConfigHome environment(configHome.path());

    SettingsController controller;
    QObject *spatial = spatialAudioController(controller);
    QVERIFY2(spatial, "SettingsController must expose its Spatial Audio configuration controller");
    QVERIFY(!spatial->property("spatialAudioEnabled").toBool());
    QVERIFY(spatial->property("spatialAudioConfigValid").toBool());
    QVERIFY(spatial->property("spatialAudioConfigError").toString().isEmpty());
    QVERIFY(!QFileInfo::exists(configPath(configHome.path())));
}

void SettingsSpatialAudioControllerTest::missingEnabledKeyDefaultsDisabled()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    const XdgConfigHome environment(configHome.path());
    QVERIFY(writeOrdinary(configPath(configHome.path()), QByteArrayLiteral(R"({"future":7})")));

    SettingsController controller;
    QObject *spatial = spatialAudioController(controller);
    QVERIFY(spatial);
    QVERIFY(!spatial->property("spatialAudioEnabled").toBool());
    QVERIFY(spatial->property("spatialAudioConfigValid").toBool());
    QVERIFY(spatial->property("spatialAudioConfigError").toString().isEmpty());
}

void SettingsSpatialAudioControllerTest::enabledValuesAreLoaded_data()
{
    QTest::addColumn<bool>("enabled");
    QTest::newRow("enabled") << true;
    QTest::newRow("disabled") << false;
}

void SettingsSpatialAudioControllerTest::enabledValuesAreLoaded()
{
    QFETCH(bool, enabled);
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    const XdgConfigHome environment(configHome.path());
    const QByteArray contents = enabled ? QByteArrayLiteral(R"({"enabled":true})")
                                        : QByteArrayLiteral(R"({"enabled":false})");
    QVERIFY(writeOrdinary(configPath(configHome.path()), contents));

    SettingsController controller;
    QObject *spatial = spatialAudioController(controller);
    QVERIFY(spatial);
    QCOMPARE(spatial->property("spatialAudioEnabled").toBool(), enabled);
    QVERIFY(spatial->property("spatialAudioConfigValid").toBool());
}

void SettingsSpatialAudioControllerTest::malformedConfigurationIsSafelyDisabledWithDiagnostic()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    const XdgConfigHome environment(configHome.path());
    QVERIFY(writeOrdinary(configPath(configHome.path()), QByteArrayLiteral("{")));

    SettingsController controller;
    QObject *spatial = spatialAudioController(controller);
    QVERIFY(spatial);
    QVERIFY(!spatial->property("spatialAudioEnabled").toBool());
    QVERIFY(!spatial->property("spatialAudioConfigValid").toBool());
    QVERIFY(!spatial->property("spatialAudioConfigError").toString().isEmpty());
}

void SettingsSpatialAudioControllerTest::malformedConfigurationCanBeReplacedByToggle()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    const XdgConfigHome environment(configHome.path());
    const QString path = configPath(configHome.path());
    QVERIFY(writeOrdinary(path, QByteArrayLiteral("{")));

    SettingsController controller;
    QObject *spatial = spatialAudioController(controller);
    QVERIFY(spatial);
    QVERIFY(!spatial->property("spatialAudioEnabled").toBool());
    QVERIFY(!spatial->property("spatialAudioConfigValid").toBool());
    QVERIFY(invokeSetEnabled(spatial, true));

    QCOMPARE(readObject(path).value(QStringLiteral("enabled")).toBool(), true);
    QVERIFY(spatial->property("spatialAudioConfigValid").toBool());
    QVERIFY(spatial->property("spatialAudioConfigError").toString().isEmpty());
}

void SettingsSpatialAudioControllerTest::toggleWritesBothStatesAndCreatesParentDirectory()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    const XdgConfigHome environment(configHome.path());
    const QString path = configPath(configHome.path());
    QVERIFY(!QFileInfo::exists(QFileInfo(path).absolutePath()));

    SettingsController controller;
    QObject *spatial = spatialAudioController(controller);
    QVERIFY(spatial);
    bool invoked = false;
    QVERIFY(invokeSetEnabled(spatial, true, &invoked));
    QVERIFY(invoked);
    QVERIFY(QFileInfo(path).isFile());
    QCOMPARE(readObject(path).value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(spatial->property("spatialAudioEnabled").toBool(), true);

    QVERIFY(invokeSetEnabled(spatial, false, &invoked));
    QVERIFY(invoked);
    QCOMPARE(readObject(path).value(QStringLiteral("enabled")).toBool(), false);
    QCOMPARE(spatial->property("spatialAudioEnabled").toBool(), false);
}

void SettingsSpatialAudioControllerTest::preservesUnknownConfigurationFields()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    const XdgConfigHome environment(configHome.path());
    const QString path = configPath(configHome.path());
    QVERIFY(writeOrdinary(path,
                          QByteArrayLiteral(R"({"enabled":"invalid","futureOption":123})")));

    SettingsController controller;
    QObject *spatial = spatialAudioController(controller);
    QVERIFY(spatial);
    QVERIFY(!spatial->property("spatialAudioConfigValid").toBool());
    QVERIFY(invokeSetEnabled(spatial, true));

    const QJsonObject saved = readObject(path);
    QCOMPARE(saved.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(saved.value(QStringLiteral("futureOption")).toInt(), 123);
    QVERIFY(spatial->property("spatialAudioConfigValid").toBool());
}

void SettingsSpatialAudioControllerTest::failedPersistenceKeepsLastPersistedState()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    const XdgConfigHome environment(configHome.path());
    const QString path = configPath(configHome.path());
    QVERIFY(writeOrdinary(path, QByteArrayLiteral(R"({"enabled":true})")));

    SettingsController controller;
    QObject *spatial = spatialAudioController(controller);
    QVERIFY(spatial);
    QVERIFY(spatial->property("spatialAudioEnabled").toBool());

    const QString audioDirectory = QFileInfo(path).absolutePath();
    const QString displacedDirectory = audioDirectory + QStringLiteral(".saved");
    QVERIFY(QDir().rename(audioDirectory, displacedDirectory));
    QFile obstruction(audioDirectory);
    QVERIFY(obstruction.open(QIODevice::WriteOnly));
    QCOMPARE(obstruction.write(QByteArrayLiteral("blocked")), qsizetype(7));
    obstruction.close();

    QVERIFY(!invokeSetEnabled(spatial, false));
    QVERIFY(spatial->property("spatialAudioEnabled").toBool());
    QVERIFY(!spatial->property("spatialAudioConfigError").toString().isEmpty());
    QCOMPARE(readObject(QDir(displacedDirectory).filePath(QStringLiteral("spatial.json")))
                 .value(QStringLiteral("enabled")).toBool(), true);
}

void SettingsSpatialAudioControllerTest::externalWritesAndAtomicReplacementUpdateState()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    const XdgConfigHome environment(configHome.path());
    const QString path = configPath(configHome.path());

    SettingsController controller;
    QObject *spatial = spatialAudioController(controller);
    QVERIFY(spatial);
    QSignalSpy changes(spatial, SIGNAL(configurationChanged()));
    QVERIFY(changes.isValid());

    QVERIFY(writeOrdinary(path, QByteArrayLiteral(R"({"enabled":true})")));
    QTRY_VERIFY_WITH_TIMEOUT(spatial->property("spatialAudioEnabled").toBool(), 3000);
    QCOMPARE(changes.count(), 1);

    QVERIFY(writeAtomically(path, QByteArrayLiteral(R"({"enabled":false})")));
    QTRY_VERIFY_WITH_TIMEOUT(!spatial->property("spatialAudioEnabled").toBool(), 3000);
    QCOMPARE(changes.count(), 2);
}

void SettingsSpatialAudioControllerTest::deletingConfigurationDisablesSpatialAudio()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    const XdgConfigHome environment(configHome.path());
    const QString path = configPath(configHome.path());
    QVERIFY(writeOrdinary(path, QByteArrayLiteral(R"({"enabled":true})")));

    SettingsController controller;
    QObject *spatial = spatialAudioController(controller);
    QVERIFY(spatial);
    QVERIFY(spatial->property("spatialAudioEnabled").toBool());
    QVERIFY(QFile::remove(path));

    QTRY_VERIFY_WITH_TIMEOUT(!spatial->property("spatialAudioEnabled").toBool(), 3000);
    QVERIFY(spatial->property("spatialAudioConfigValid").toBool());
    QVERIFY(spatial->property("spatialAudioConfigError").toString().isEmpty());
}

void SettingsSpatialAudioControllerTest::equivalentFilesystemEventsDoNotRepeatSemanticChanges()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    const XdgConfigHome environment(configHome.path());
    const QString path = configPath(configHome.path());

    SettingsController controller;
    QObject *spatial = spatialAudioController(controller);
    QVERIFY(spatial);
    QSignalSpy changes(spatial, SIGNAL(configurationChanged()));
    QVERIFY(changes.isValid());

    QVERIFY(writeAtomically(path, QByteArrayLiteral(R"({"enabled":false})")));
    QTest::qWait(350);
    QVERIFY(!spatial->property("spatialAudioEnabled").toBool());
    QCOMPARE(changes.count(), 0);

    QVERIFY(writeAtomically(path, QByteArrayLiteral(R"({"enabled":false})")));
    QTest::qWait(350);
    QCOMPARE(changes.count(), 0);
}

QTEST_GUILESS_MAIN(SettingsSpatialAudioControllerTest)
#include "SettingsSpatialAudioControllerTest.moc"
