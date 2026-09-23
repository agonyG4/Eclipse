#include "theme/ThemeController.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaProperty>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <future>
#include <thread>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <unistd.h>

namespace {

QString writeConfig(QTemporaryDir &directory, const QByteArray &contents)
{
    const QString path = directory.filePath(QStringLiteral("theme.json"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    file.write(contents);
    return path;
}

QJsonObject readConfig(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool writeConfigAtomically(const QString &path, const QJsonObject &object)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

int acquireConfigLock(const QString &path)
{
    const QByteArray lockPath = QFile::encodeName(path + QStringLiteral(".lock"));
    const int descriptor = ::open(lockPath.constData(), O_CREAT | O_RDWR | O_CLOEXEC, 0666);
    if (descriptor < 0)
        return -1;
    if (::flock(descriptor, LOCK_EX) == 0)
        return descriptor;
    ::close(descriptor);
    return -1;
}

void releaseConfigLock(int descriptor)
{
    if (descriptor >= 0) {
        ::flock(descriptor, LOCK_UN);
        ::close(descriptor);
    }
}

} // namespace

class ThemeControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void qtPropertiesEnforceAppearanceOwnership();
    void usesLegacyDefaultsWhenConfigIsMissing();
    void iconAppearanceDefaultsToDefault();
    void iconAppearanceNormalizesCanonicalValues();
    void iconAppearanceRejectsInvalidValues();
    void iconAppearanceResetsOnCompleteReplacement();
    void savePersistsOnlyUnmigratedLegacyFields();
    void savePreservesRustAndUnknownKeys();
    void staleAppearanceStateCannotOverwriteRustValuesOnLegacySave();
    void saveWaitsForRustTransactionAndPreservesBothUpdates();
    void saveDoesNotOverwriteMalformedConfig();
    void failedAtomicSaveLeavesPreviousConfigIntact();
    void legacyIconFieldsDoNotMigrateToAppearance();
    void iconAppearancePreservesUnrelatedThemeState();
    void loadsLegacyConfigValues();
    void customAccentPreservesThemeAndShellState();
    void reloadsExternalConfigReplacement();
    void completeExternalReplacementUsesThemeDefaults();
    void invalidExternalReplacementUsesThemeDefaults();
    void keepsLastGoodStateWhenExternalConfigIsInvalid();
    void legacyLightConfigurationMigratesInMemory();
    void legacyDarkConfigurationMigratesInMemory();
    void validShellStylesRemainCompatible();
    void invalidShellStyleUsesDefault();
    void saveWritesOnlyUnmigratedFields();
    void legacySetThemeModeSelectsExplicitPreference();
    void automaticPlatformChangeUpdatesEffectiveModeOnly();
};

void ThemeControllerTest::qtPropertiesEnforceAppearanceOwnership()
{
    const QMetaObject *metaObject = &ThemeController::staticMetaObject;
    const QByteArray applyConfigSignature = QMetaObject::normalizedSignature("applyConfig(QVariantMap)");
    QVERIFY2(metaObject->indexOfMethod(applyConfigSignature.constData()) < 0,
             "applyConfig must not be exposed through the Qt meta-object");
    QVERIFY(metaObject->indexOfMethod("reload()") >= 0);
    QVERIFY(metaObject->indexOfMethod("save()") >= 0);

    const QStringList appearanceProperties {
        QStringLiteral("themeMode"),
        QStringLiteral("themePreference"),
        QStringLiteral("accentHex"),
        QStringLiteral("iconAppearance"),
    };
    for (const QString &name : appearanceProperties) {
        const int propertyIndex = metaObject->indexOfProperty(name.toLatin1().constData());
        QVERIFY2(propertyIndex >= 0, qPrintable(name));
        QVERIFY2(!metaObject->property(propertyIndex).isWritable(), qPrintable(name));
    }

    const QStringList transitionalProperties {
        QStringLiteral("shellStyle"),
        QStringLiteral("iconStyle"),
        QStringLiteral("iconTheme"),
        QStringLiteral("audioOsdStyle"),
    };
    for (const QString &name : transitionalProperties) {
        const int propertyIndex = metaObject->indexOfProperty(name.toLatin1().constData());
        QVERIFY2(propertyIndex >= 0, qPrintable(name));
        QVERIFY2(metaObject->property(propertyIndex).isWritable(), qPrintable(name));
    }
}

void ThemeControllerTest::usesLegacyDefaultsWhenConfigIsMissing()
{
    ThemeController controller(QStringLiteral("/tmp/astrea-settings-missing-theme.json"));

    QCOMPARE(controller.themeMode(), 0);
    QCOMPARE(controller.themePreference(), QStringLiteral("auto"));
    QCOMPARE(controller.shellStyle(), 1);
    QCOMPARE(controller.iconStyle(), 0);
    QCOMPARE(controller.iconTheme(), QStringLiteral("dark"));
    QCOMPARE(controller.iconAppearance(), QStringLiteral("default"));
    QCOMPARE(controller.accentHex(), QStringLiteral("#0a84ff"));
    QCOMPARE(controller.audioOsdStyle(), 0);
}

void ThemeControllerTest::iconAppearanceDefaultsToDefault()
{
    ThemeController controller(QStringLiteral("/tmp/astrea-settings-missing-theme.json"));

    QCOMPARE(controller.iconAppearance(), QStringLiteral("default"));
}

void ThemeControllerTest::iconAppearanceNormalizesCanonicalValues()
{
    ThemeController controller(QStringLiteral("/tmp/astrea-settings-missing-theme.json"));

    controller.setIconAppearance(QStringLiteral(" MONOCHROME "));
    QCOMPARE(controller.iconAppearance(), QStringLiteral("monochrome"));

    controller.setIconAppearance(QStringLiteral("TiNtEd"));
    QCOMPARE(controller.iconAppearance(), QStringLiteral("tinted"));

    controller.setIconAppearance(QStringLiteral("DEFAULT"));
    QCOMPARE(controller.iconAppearance(), QStringLiteral("default"));
}

void ThemeControllerTest::iconAppearanceRejectsInvalidValues()
{
    ThemeController controller(QStringLiteral("/tmp/astrea-settings-missing-theme.json"));

    controller.setIconAppearance(QStringLiteral("tinted"));
    controller.setIconAppearance(QStringLiteral("unsupported"));
    QCOMPARE(controller.iconAppearance(), QStringLiteral("default"));

    controller.applyConfig({{QStringLiteral("icon_appearance"), QStringLiteral("dark")}});
    QCOMPARE(controller.iconAppearance(), QStringLiteral("default"));
}

void ThemeControllerTest::iconAppearanceResetsOnCompleteReplacement()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(directory, R"({"icon_appearance":"tinted"})");
    QVERIFY(!path.isEmpty());

    ThemeController controller(path);
    QCOMPARE(controller.iconAppearance(), QStringLiteral("tinted"));

    QFile replacement(path);
    QVERIFY(replacement.open(QIODevice::WriteOnly | QIODevice::Truncate));
    replacement.write(QByteArrayLiteral(R"({"theme_preference":"light"})"));
    replacement.close();

    QTRY_COMPARE_WITH_TIMEOUT(controller.iconAppearance(), QStringLiteral("default"), 1500);
}

void ThemeControllerTest::savePersistsOnlyUnmigratedLegacyFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("theme.json"));
    ThemeController controller(path, nullptr, [] { return Qt::ColorScheme::Light; });

    controller.applyConfig({
        {QStringLiteral("theme_preference"), QStringLiteral("dark")},
        {QStringLiteral("shell_style"), 2},
        {QStringLiteral("accent"), QStringLiteral("#30d158")},
        {QStringLiteral("icon_appearance"), QStringLiteral("MONOCHROME")},
        {QStringLiteral("icon_style"), 1},
        {QStringLiteral("icon_theme"), QStringLiteral("dark")},
        {QStringLiteral("audio_osd_style"), 1},
    });
    controller.save();

    const QJsonObject saved = readConfig(path);
    QVERIFY(!saved.contains(QStringLiteral("icon_appearance")));
    QVERIFY(!saved.contains(QStringLiteral("theme_preference")));
    QVERIFY(!saved.contains(QStringLiteral("accent")));
    QCOMPARE(saved.value(QStringLiteral("icon_style")).toInt(), 1);
    QCOMPARE(saved.value(QStringLiteral("icon_theme")).toString(), QStringLiteral("dark"));
    QCOMPARE(saved.value(QStringLiteral("shell_style")).toInt(), 2);
    QCOMPARE(saved.value(QStringLiteral("audio_osd_style")).toInt(), 1);
}

void ThemeControllerTest::savePreservesRustAndUnknownKeys()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(
        directory,
        R"({"accent":"#123456","theme_preference":"auto","icon_appearance":"default","system_icon_theme":"Breeze","future_setting":{"enabled":true},"icon_theme":"legacy"})");
    QVERIFY(!path.isEmpty());

    ThemeController controller(path);
    controller.setAccentHex(QStringLiteral("#30d158"));
    controller.save();

    const QJsonObject saved = readConfig(path);
    QCOMPARE(saved.value(QStringLiteral("system_icon_theme")).toString(), QStringLiteral("Breeze"));
    QCOMPARE(saved.value(QStringLiteral("theme_preference")).toString(), QStringLiteral("auto"));
    QCOMPARE(saved.value(QStringLiteral("icon_appearance")).toString(), QStringLiteral("default"));
    QCOMPARE(saved.value(QStringLiteral("future_setting")).toObject().value(QStringLiteral("enabled")),
             QJsonValue(true));
    QCOMPARE(saved.value(QStringLiteral("icon_theme")).toString(), QStringLiteral("legacy"));
    QCOMPARE(saved.value(QStringLiteral("accent")).toString(), QStringLiteral("#123456"));
}

void ThemeControllerTest::staleAppearanceStateCannotOverwriteRustValuesOnLegacySave()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(
        directory,
        R"({"theme_preference":"light","theme":"light","theme_mode":1,"accent":"#123456","icon_appearance":"monochrome","system_icon_theme":"Breeze","shell_style":1,"future_setting":true})");
    QVERIFY(!path.isEmpty());

    ThemeController controller(path);
    QCOMPARE(controller.accentHex(), QStringLiteral("#123456"));
    controller.setAccentHex(QStringLiteral("#aaaaaa"));
    controller.setThemePreference(QStringLiteral("dark"));
    controller.setIconAppearance(QStringLiteral("tinted"));
    controller.setShellStyle(2);

    // Model a Rust Appearance commit after ThemeController loaded its projection.
    QVERIFY(writeConfigAtomically(
        path,
        {{QStringLiteral("theme_preference"), QStringLiteral("auto")},
         {QStringLiteral("theme"), QStringLiteral("legacy-light")},
         {QStringLiteral("theme_mode"), 1},
         {QStringLiteral("accent"), QStringLiteral("#30d158")},
         {QStringLiteral("icon_appearance"), QStringLiteral("default")},
         {QStringLiteral("system_icon_theme"), QStringLiteral("Papirus")},
         {QStringLiteral("shell_style"), 1},
         {QStringLiteral("future_setting"), true}}));

    controller.save();

    const QJsonObject saved = readConfig(path);
    QCOMPARE(saved.value(QStringLiteral("theme_preference")).toString(), QStringLiteral("auto"));
    QCOMPARE(saved.value(QStringLiteral("theme")).toString(), QStringLiteral("legacy-light"));
    QCOMPARE(saved.value(QStringLiteral("theme_mode")).toInt(), 1);
    QCOMPARE(saved.value(QStringLiteral("accent")).toString(), QStringLiteral("#30d158"));
    QCOMPARE(saved.value(QStringLiteral("icon_appearance")).toString(), QStringLiteral("default"));
    QCOMPARE(saved.value(QStringLiteral("system_icon_theme")).toString(), QStringLiteral("Papirus"));
    QCOMPARE(saved.value(QStringLiteral("shell_style")).toInt(), 2);
    QCOMPARE(saved.value(QStringLiteral("future_setting")).toBool(), true);
}

void ThemeControllerTest::saveWaitsForRustTransactionAndPreservesBothUpdates()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(
        directory,
        R"({"accent":"#123456","theme_preference":"auto","icon_appearance":"default","system_icon_theme":"Before","future_setting":{"enabled":true}})");
    QVERIFY(!path.isEmpty());

    // The test-owned lock and read model Rust holding the common advisory lock.
    const int lock = acquireConfigLock(path);
    QVERIFY(lock >= 0);
    QJsonObject rustObject = readConfig(path);
    QCOMPARE(rustObject.value(QStringLiteral("system_icon_theme")).toString(),
             QStringLiteral("Before"));

    std::promise<void> saveAttempted;
    std::promise<void> saveCompleted;
    auto completed = saveCompleted.get_future();
    std::thread cppWriter([&] {
        ThemeController controller(path);
        controller.setShellStyle(2);
        controller.setIconStyle(1);
        controller.setIconTheme(QStringLiteral("legacy-dark"));
        controller.setAudioOsdStyle(1);
        saveAttempted.set_value();
        controller.save();
        saveCompleted.set_value();
    });

    saveAttempted.get_future().wait();
    const bool completedWhileRustHeldLock =
        completed.wait_for(std::chrono::milliseconds(250)) == std::future_status::ready;

    rustObject.insert(QStringLiteral("system_icon_theme"), QStringLiteral("Nordic"));
    const bool rustCommitSucceeded = writeConfigAtomically(path, rustObject);
    releaseConfigLock(lock);
    cppWriter.join();

    QVERIFY(rustCommitSucceeded);
    QVERIFY(!completedWhileRustHeldLock);
    const QJsonObject saved = readConfig(path);
    QCOMPARE(saved.value(QStringLiteral("accent")).toString(), QStringLiteral("#123456"));
    QCOMPARE(saved.value(QStringLiteral("theme_preference")).toString(), QStringLiteral("auto"));
    QCOMPARE(saved.value(QStringLiteral("icon_appearance")).toString(), QStringLiteral("default"));
    QCOMPARE(saved.value(QStringLiteral("system_icon_theme")).toString(), QStringLiteral("Nordic"));
    QCOMPARE(saved.value(QStringLiteral("shell_style")).toInt(), 2);
    QCOMPARE(saved.value(QStringLiteral("icon_style")).toInt(), 1);
    QCOMPARE(saved.value(QStringLiteral("icon_theme")).toString(), QStringLiteral("legacy-dark"));
    QCOMPARE(saved.value(QStringLiteral("audio_osd_style")).toInt(), 1);
    QCOMPARE(saved.value(QStringLiteral("future_setting")).toObject()
                 .value(QStringLiteral("enabled")), QJsonValue(true));
}

void ThemeControllerTest::saveDoesNotOverwriteMalformedConfig()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QByteArray malformed = QByteArrayLiteral("{ malformed");
    const QString path = writeConfig(directory, malformed);
    QVERIFY(!path.isEmpty());

    ThemeController controller(path);
    controller.setAccentHex(QStringLiteral("#30d158"));
    controller.save();

    QFile saved(path);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QCOMPARE(saved.readAll(), malformed);
}

void ThemeControllerTest::failedAtomicSaveLeavesPreviousConfigIntact()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QByteArray original = QByteArrayLiteral(R"({"system_icon_theme":"Before"})");
    const QString path = writeConfig(directory, original);
    QVERIFY(!path.isEmpty());

    struct rlimit previousLimit {};
    if (::getrlimit(RLIMIT_FSIZE, &previousLimit) != 0)
        QSKIP("RLIMIT_FSIZE is unavailable");
    const auto oldSignalHandler = std::signal(SIGXFSZ, SIG_IGN);
    struct rlimit limited = previousLimit;
    const rlim_t targetLimit = 64;
    limited.rlim_cur = previousLimit.rlim_max == RLIM_INFINITY
        ? targetLimit : std::min(targetLimit, previousLimit.rlim_max);
    if (::setrlimit(RLIMIT_FSIZE, &limited) != 0) {
        std::signal(SIGXFSZ, oldSignalHandler);
        QSKIP("Could not set a small file-size limit");
    }

    ThemeController controller(path);
    controller.setAccentHex(QStringLiteral("#30d158"));
    controller.save();

    const int restoreResult = ::setrlimit(RLIMIT_FSIZE, &previousLimit);
    std::signal(SIGXFSZ, oldSignalHandler);
    QVERIFY(restoreResult == 0);
    QFile saved(path);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QCOMPARE(saved.readAll(), original);
}

void ThemeControllerTest::legacyIconFieldsDoNotMigrateToAppearance()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(directory,
                                     R"({"icon_style":1,"icon_theme":"dark"})");
    QVERIFY(!path.isEmpty());

    ThemeController controller(path);

    QCOMPARE(controller.iconAppearance(), QStringLiteral("default"));
}

void ThemeControllerTest::iconAppearancePreservesUnrelatedThemeState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(directory,
                                     R"({"theme_preference":"light","shell_style":2,"accent":"#123456"})");
    QVERIFY(!path.isEmpty());

    ThemeController controller(path, nullptr, [] { return Qt::ColorScheme::Dark; });
    controller.setIconAppearance(QStringLiteral("tinted"));

    QCOMPARE(controller.iconAppearance(), QStringLiteral("tinted"));
    QCOMPARE(controller.themePreference(), QStringLiteral("light"));
    QCOMPARE(controller.shellStyle(), 2);
    QCOMPARE(controller.accentHex(), QStringLiteral("#123456"));

    controller.setAccentHex(QStringLiteral("#30d158"));
    QCOMPARE(controller.iconAppearance(), QStringLiteral("tinted"));
    QCOMPARE(controller.themePreference(), QStringLiteral("light"));
    QCOMPARE(controller.shellStyle(), 2);
    QCOMPARE(controller.accentHex(), QStringLiteral("#30d158"));
}

void ThemeControllerTest::loadsLegacyConfigValues()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("theme.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream << R"({
        "theme_mode": 1,
        "shell_style": 2,
        "icon_style": 1,
        "icon_theme": "dark",
        "accent": "#30d158",
        "audio_osd_style": 1
    })";
    file.close();

    ThemeController controller(path);

    QCOMPARE(controller.themeMode(), 1);
    QCOMPARE(controller.shellStyle(), 2);
    QCOMPARE(controller.iconStyle(), 1);
    QCOMPARE(controller.iconTheme(), QStringLiteral("dark"));
    QCOMPARE(controller.accentHex(), QStringLiteral("#30d158"));
    QCOMPARE(controller.audioOsdStyle(), 1);
}

void ThemeControllerTest::customAccentPreservesThemeAndShellState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = writeConfig(directory,
                                     R"({"theme_preference":"light","shell_style":2,"accent":"#123456"})");
    QVERIFY(!path.isEmpty());

    ThemeController controller(path, nullptr, [] { return Qt::ColorScheme::Dark; });

    QCOMPARE(controller.accentHex(), QStringLiteral("#123456"));
    QCOMPARE(controller.themePreference(), QStringLiteral("light"));
    QCOMPARE(controller.shellStyle(), 2);
    QCOMPARE(readConfig(path).value(QStringLiteral("accent")).toString(),
             QStringLiteral("#123456"));

    controller.setAccentHex(QStringLiteral("#30d158"));
    QCOMPARE(controller.accentHex(), QStringLiteral("#30d158"));
    QCOMPARE(controller.themePreference(), QStringLiteral("light"));
    QCOMPARE(controller.shellStyle(), 2);

    controller.save();
    const QJsonObject saved = readConfig(path);
    QCOMPARE(saved.value(QStringLiteral("accent")).toString(), QStringLiteral("#123456"));
    QCOMPARE(saved.value(QStringLiteral("theme_preference")).toString(), QStringLiteral("light"));
    QCOMPARE(saved.value(QStringLiteral("shell_style")).toInt(), 2);
}

void ThemeControllerTest::reloadsExternalConfigReplacement()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("theme.json"));
    QFile initial(path);
    QVERIFY(initial.open(QIODevice::WriteOnly));
    initial.write(R"({"theme":"dark","shell_style":0})");
    initial.close();

    ThemeController controller(path);
    QCOMPARE(controller.themeMode(), 0);
    QSignalSpy modeSpy(&controller, &ThemeController::themeModeChanged);

    QFile replacement(path);
    QVERIFY(replacement.open(QIODevice::WriteOnly | QIODevice::Truncate));
    replacement.write(R"({"theme":"light","shell_style":2,"accent":"#30d158"})");
    replacement.close();

    QTRY_COMPARE_WITH_TIMEOUT(controller.themeMode(), 1, 1500);
    QCOMPARE(controller.shellStyle(), 2);
    QCOMPARE(controller.accentHex(), QStringLiteral("#30d158"));
    QCOMPARE(modeSpy.count(), 1);
}

void ThemeControllerTest::completeExternalReplacementUsesThemeDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(directory, R"({"theme_preference":"light","shell_style":0})");
    QVERIFY(!path.isEmpty());

    ThemeController controller(path, nullptr, [] { return Qt::ColorScheme::Dark; });
    QCOMPARE(controller.themePreference(), QStringLiteral("light"));
    QCOMPARE(controller.themeMode(), 1);
    QCOMPARE(controller.shellStyle(), 0);

    QFile replacement(path);
    QVERIFY(replacement.open(QIODevice::WriteOnly | QIODevice::Truncate));
    replacement.write(QByteArrayLiteral("{}"));
    replacement.close();

    QTRY_COMPARE_WITH_TIMEOUT(controller.themePreference(), QStringLiteral("auto"), 1500);
    QTRY_COMPARE_WITH_TIMEOUT(controller.shellStyle(), 1, 1500);
    QCOMPARE(controller.themeMode(), 0);
}

void ThemeControllerTest::invalidExternalReplacementUsesThemeDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(directory, R"({"theme_preference":"light","shell_style":0})");
    QVERIFY(!path.isEmpty());

    ThemeController controller(path, nullptr, [] { return Qt::ColorScheme::Dark; });
    QCOMPARE(controller.themePreference(), QStringLiteral("light"));
    QCOMPARE(controller.themeMode(), 1);
    QCOMPARE(controller.shellStyle(), 0);

    QFile replacement(path);
    QVERIFY(replacement.open(QIODevice::WriteOnly | QIODevice::Truncate));
    replacement.write(QByteArrayLiteral(
        R"({"theme_preference":"invalid","theme":"invalid","theme_mode":99,"shell_style":99})"));
    replacement.close();

    QTRY_COMPARE_WITH_TIMEOUT(controller.themePreference(), QStringLiteral("auto"), 1500);
    QTRY_COMPARE_WITH_TIMEOUT(controller.shellStyle(), 1, 1500);
    QCOMPARE(controller.themeMode(), 0);
}

void ThemeControllerTest::keepsLastGoodStateWhenExternalConfigIsInvalid()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("theme.json"));
    QFile initial(path);
    QVERIFY(initial.open(QIODevice::WriteOnly));
    initial.write(R"({"theme":"light","shell_style":2})");
    initial.close();

    ThemeController controller(path);
    QCOMPARE(controller.themeMode(), 1);
    QCOMPARE(controller.shellStyle(), 2);

    QFile invalid(path);
    QVERIFY(invalid.open(QIODevice::WriteOnly | QIODevice::Truncate));
    invalid.write(QByteArrayLiteral("{invalid"));
    invalid.close();

    QTest::qWait(450);
    QCOMPARE(controller.themeMode(), 1);
    QCOMPARE(controller.shellStyle(), 2);
}

void ThemeControllerTest::legacyLightConfigurationMigratesInMemory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(directory, R"({"theme":"light","shell_style":1})");
    QVERIFY(!path.isEmpty());

    ThemeController controller(path);

    QCOMPARE(controller.themePreference(), QStringLiteral("light"));
    QCOMPARE(controller.themeMode(), 1);
}

void ThemeControllerTest::legacyDarkConfigurationMigratesInMemory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(directory, R"({"theme":"dark","shell_style":1})");
    QVERIFY(!path.isEmpty());

    ThemeController controller(path);

    QCOMPARE(controller.themePreference(), QStringLiteral("dark"));
    QCOMPARE(controller.themeMode(), 0);
}

void ThemeControllerTest::validShellStylesRemainCompatible()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(directory, R"({"theme":"dark","shell_style":0})");
    QVERIFY(!path.isEmpty());

    ThemeController transparentController(path);
    QCOMPARE(transparentController.shellStyle(), 0);

    const QString frostedPath = writeConfig(directory, R"({"theme":"dark","shell_style":2})");
    QVERIFY(!frostedPath.isEmpty());
    ThemeController frostedController(frostedPath);
    QCOMPARE(frostedController.shellStyle(), 2);
}

void ThemeControllerTest::invalidShellStyleUsesDefault()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(directory, R"({"theme":"dark","shell_style":99})");
    QVERIFY(!path.isEmpty());

    ThemeController controller(path);

    QCOMPARE(controller.shellStyle(), 1);
}

void ThemeControllerTest::saveWritesOnlyUnmigratedFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(
        directory,
        R"({"theme_preference":"auto","theme":"legacy-dark","theme_mode":0,"accent":"#123456","icon_appearance":"monochrome","system_icon_theme":"Breeze"})");
    QVERIFY(!path.isEmpty());
    ThemeController controller(path, nullptr, [] { return Qt::ColorScheme::Light; });

    controller.applyConfig({
        {QStringLiteral("theme_preference"), QStringLiteral("auto")},
        {QStringLiteral("shell_style"), 2},
        {QStringLiteral("accent"), QStringLiteral("#30d158")},
        {QStringLiteral("icon_style"), 1},
        {QStringLiteral("icon_theme"), QStringLiteral("dark")},
        {QStringLiteral("audio_osd_style"), 1},
    });
    controller.save();

    const QJsonObject object = readConfig(path);
    QCOMPARE(object.value(QStringLiteral("theme_preference")).toString(), QStringLiteral("auto"));
    QCOMPARE(object.value(QStringLiteral("theme")).toString(), QStringLiteral("legacy-dark"));
    QCOMPARE(object.value(QStringLiteral("theme_mode")).toInt(), 0);
    QCOMPARE(object.value(QStringLiteral("shell_style")).toInt(), 2);
    QCOMPARE(object.value(QStringLiteral("accent")).toString(), QStringLiteral("#123456"));
    QCOMPARE(object.value(QStringLiteral("icon_appearance")).toString(), QStringLiteral("monochrome"));
    QCOMPARE(object.value(QStringLiteral("system_icon_theme")).toString(), QStringLiteral("Breeze"));
    QCOMPARE(object.value(QStringLiteral("icon_style")).toInt(), 1);
    QCOMPARE(object.value(QStringLiteral("icon_theme")).toString(), QStringLiteral("dark"));
    QCOMPARE(object.value(QStringLiteral("audio_osd_style")).toInt(), 1);
}

void ThemeControllerTest::legacySetThemeModeSelectsExplicitPreference()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ThemeController controller(directory.filePath(QStringLiteral("missing.json")));

    controller.setThemeMode(1);
    QCOMPARE(controller.themePreference(), QStringLiteral("light"));
    QCOMPARE(controller.themeMode(), 1);

    controller.setThemeMode(0);
    QCOMPARE(controller.themePreference(), QStringLiteral("dark"));
    QCOMPARE(controller.themeMode(), 0);
}

void ThemeControllerTest::automaticPlatformChangeUpdatesEffectiveModeOnly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Qt::ColorScheme scheme = Qt::ColorScheme::Dark;
    ThemeController controller(directory.filePath(QStringLiteral("missing.json")), nullptr,
                               [&scheme] { return scheme; });

    QCOMPARE(controller.themePreference(), QStringLiteral("auto"));
    QCOMPARE(controller.themeMode(), 0);

    scheme = Qt::ColorScheme::Light;
    QVERIFY(QMetaObject::invokeMethod(&controller, "handlePlatformColorSchemeChanged",
                                      Qt::DirectConnection));

    QCOMPARE(controller.themePreference(), QStringLiteral("auto"));
    QCOMPARE(controller.themeMode(), 1);
}

QTEST_GUILESS_MAIN(ThemeControllerTest)
#include "ThemeControllerTest.moc"
