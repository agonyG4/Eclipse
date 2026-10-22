#include "core/SettingsController.hpp"
#include "theme/ThemeController.hpp"

#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlError>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {

class GamingTestEnvironment final {
public:
    GamingTestEnvironment()
        : m_previousHome(qgetenv("HOME"))
        , m_previousConfig(qgetenv("XDG_CONFIG_HOME"))
        , m_previousData(qgetenv("XDG_DATA_HOME"))
        , m_previousState(qgetenv("XDG_STATE_HOME"))
        , m_previousPath(qgetenv("PATH"))
        , m_previousCalls(qgetenv("FAKE_GAMING_CALLS"))
        , m_previousFailure(qgetenv("FAKE_GAMING_FAILURE"))
    {
        if (!m_root.isValid())
            return;
        const QString base = m_root.path();
        m_home = QDir(base).filePath(QStringLiteral("home"));
        m_config = QDir(base).filePath(QStringLiteral("config"));
        m_bin = QDir(base).filePath(QStringLiteral("bin"));
        m_calls = QDir(base).filePath(QStringLiteral("calls"));
        for (const QString &path : {m_home, m_config, m_bin})
            QDir().mkpath(path);
        qputenv("HOME", m_home.toUtf8());
        qputenv("XDG_CONFIG_HOME", m_config.toUtf8());
        qputenv("XDG_DATA_HOME", QDir(base).filePath(QStringLiteral("data")).toUtf8());
        qputenv("XDG_STATE_HOME", QDir(base).filePath(QStringLiteral("state")).toUtf8());
        qputenv("PATH", m_bin.toUtf8());
        qputenv("FAKE_GAMING_CALLS", m_calls.toUtf8());
        qputenv("FAKE_GAMING_FAILURE", "0");
    }

    ~GamingTestEnvironment()
    {
        restore("HOME", m_previousHome);
        restore("XDG_CONFIG_HOME", m_previousConfig);
        restore("XDG_DATA_HOME", m_previousData);
        restore("XDG_STATE_HOME", m_previousState);
        restore("PATH", m_previousPath);
        restore("FAKE_GAMING_CALLS", m_previousCalls);
        restore("FAKE_GAMING_FAILURE", m_previousFailure);
    }

    bool isValid() const { return m_root.isValid(); }
    QString configHome() const { return m_config; }
    QString binPath() const { return m_bin; }
    QString callsPath() const { return m_calls; }

    bool addHelpers()
    {
        const QString common = QStringLiteral(
            "#!/bin/sh\n"
            "if [ \"$1\" = get ]; then echo balanced; exit 0; fi\n"
            "if [ \"$1\" = -s ]; then exit 1; fi\n"
            "printf '%s %s\\n' \"${0##*/}\" \"$*\" >> \"$FAKE_GAMING_CALLS\"\n"
            "exit \"${FAKE_GAMING_FAILURE:-0}\"\n");
        for (const QString &program : {QStringLiteral("powerprofilesctl"),
                                      QStringLiteral("gamemoded"),
                                      QStringLiteral("gamemoderun"),
                                      QStringLiteral("mangohud"),
                                      QStringLiteral("gamescope"),
                                      QStringLiteral("wine"),
                                      QStringLiteral("proton"),
                                      QStringLiteral("astrea-gaming"),
                                      QStringLiteral("scxctl"),
                                      QStringLiteral("scx_bpfland")}) {
            if (!writeExecutable(program, common))
                return false;
        }
        return true;
    }

    bool writeExecutable(const QString &name, const QString &contents)
    {
        QFile file(QDir(m_bin).filePath(name));
        if (!file.open(QIODevice::WriteOnly) || file.write(contents.toUtf8()) < 0)
            return false;
        file.close();
        return file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                   | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                                   | QFileDevice::ExeGroup | QFileDevice::ReadOther
                                   | QFileDevice::ExeOther);
    }

    void setFailure(bool failure)
    {
        qputenv("FAKE_GAMING_FAILURE", failure ? "1" : "0");
    }

private:
    static void restore(const char *name, const QByteArray &value)
    {
        if (value.isNull())
            qunsetenv(name);
        else
            qputenv(name, value);
    }

    QTemporaryDir m_root;
    QByteArray m_previousHome;
    QByteArray m_previousConfig;
    QByteArray m_previousData;
    QByteArray m_previousState;
    QByteArray m_previousPath;
    QByteArray m_previousCalls;
    QByteArray m_previousFailure;
    QString m_home;
    QString m_config;
    QString m_bin;
    QString m_calls;
};

class SettingsPage final {
public:
    SettingsPage(SettingsController &settings, const QUrl &url)
    {
        QObject::connect(&m_engine, &QQmlEngine::warnings, &m_engine,
                         [this](const QList<QQmlError> &warnings) {
                             for (const QQmlError &warning : warnings)
                                 m_warnings.append(warning.toString());
                         });
        m_engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settings);
        m_engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &m_themeController);
        m_engine.addImportPath(QStringLiteral("qrc:/"));
        m_component = std::make_unique<QQmlComponent>(&m_engine, url);
        m_page.reset(m_component->create());
        if (auto *item = qobject_cast<QQuickItem *>(m_page.get())) {
            item->setParentItem(m_window.contentItem());
            item->setWidth(960);
            item->setHeight(800);
            m_window.resize(960, 800);
            m_window.show();
        }
    }

    bool isValid() const { return m_page && qobject_cast<QQuickItem *>(m_page.get()); }
    QString errors() const { return m_component ? m_component->errorString() : QString(); }
    QStringList warnings() const { return m_warnings; }
    QObject *object(const QString &name) const
    {
        return m_page ? m_page->findChild<QObject *>(name) : nullptr;
    }

private:
    QTemporaryDir m_themeDirectory;
    ThemeController m_themeController{m_themeDirectory.filePath(QStringLiteral("theme.json"))};
    QStringList m_warnings;
    QQmlEngine m_engine;
    QQuickWindow m_window;
    std::unique_ptr<QQmlComponent> m_component;
    std::unique_ptr<QObject> m_page;
};

bool waitForGaming(SettingsGamingController *controller)
{
    controller->refresh();
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 7000) {
        QCoreApplication::processEvents();
        if (!controller->busy()
            && (controller->stateJson() != QStringLiteral("{}")
                || !controller->lastError().isEmpty()))
            return controller->stateJson() != QStringLiteral("{}");
        QTest::qWait(10);
    }
    return false;
}

bool waitForPerformance(SettingsPerformanceController *controller)
{
    controller->refresh();
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 7000) {
        QCoreApplication::processEvents();
        if (!controller->busy()
            && (controller->stateJson() != QStringLiteral("{}")
                || !controller->lastError().isEmpty()))
            return controller->stateJson() != QStringLiteral("{}");
        QTest::qWait(10);
    }
    return false;
}

QJsonObject readJsonObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

} // namespace

class SettingsGamingQmlTest final : public QObject {
    Q_OBJECT

private slots:
    void protonAndCompatibilityOptionsPersistAndPagesReloadFromNativeState();
    void performancePreferenceAndExplicitApplyUseNativeBackend();
    void missingHelpersDisableSystemActions();
    void failedApplyAndInvalidInputRemainVisibleWithoutFalseSuccess();
};

void SettingsGamingQmlTest::protonAndCompatibilityOptionsPersistAndPagesReloadFromNativeState()
{
    GamingTestEnvironment environment;
    QVERIFY(environment.isValid());
    QVERIFY(environment.addHelpers());
    SettingsController settings;
    settings.gaming()->refresh();
    QVERIFY(waitForGaming(settings.gaming()));

    const QUrl protonUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Settings/qml/pages/gaming/Proton.qml"));
    {
        SettingsPage page(settings, protonUrl);
        QVERIFY2(page.isValid(), qPrintable(page.errors()));
        QVERIFY(waitForGaming(settings.gaming()));
        auto *launchField = page.object(QStringLiteral("steamLaunchOptionField"));
        QVERIFY(launchField);
        const QJsonObject initialSnapshot = QJsonDocument::fromJson(
            settings.gaming()->stateJson().toUtf8()).object();
        const QString launchCommand = initialSnapshot.value(QStringLiteral("status")).toObject()
                                           .value(QStringLiteral("launch_command")).toString();
        QVERIFY(launchCommand.contains(QStringLiteral("%command%")));
        QCOMPARE(launchField->property("text").toString(), launchCommand);
        QVERIFY(launchField->property("readOnly").toBool());
        QVERIFY(settings.copyGamingLaunchCommand());
        QCOMPARE(QGuiApplication::clipboard()->text(), launchCommand);

        settings.gaming()->setProtonOption(QStringLiteral("gamescope_width"),
                                            QStringLiteral("2560"));
        QTRY_VERIFY_WITH_TIMEOUT(settings.gaming()->stateJson().contains(QStringLiteral("2560")),
                                 7000);
        QTRY_VERIFY_WITH_TIMEOUT(!settings.gaming()->busy(), 7000);
        QVERIFY2(page.warnings().isEmpty(), qPrintable(page.warnings().join(QLatin1Char('\n'))));
        const QString protonPath = QDir(environment.configHome())
                                       .filePath(QStringLiteral("AstreaOS/gaming/proton.json"));
        QCOMPARE(readJsonObject(protonPath).value(QStringLiteral("gamescope_width")).toInt(), 2560);
        settings.gaming()->setProtonOption(QStringLiteral("preset"), QStringLiteral("\"hdr\""));
        QTRY_VERIFY_WITH_TIMEOUT(settings.gaming()->stateJson().contains(QStringLiteral("\"preset\":\"hdr\"")),
                                 7000);
        QTRY_VERIFY_WITH_TIMEOUT(!settings.gaming()->busy(), 7000);
    }

    const QUrl compatibilityUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Settings/qml/pages/gaming/Compatibility.qml"));
    {
        SettingsPage page(settings, compatibilityUrl);
        QVERIFY2(page.isValid(), qPrintable(page.errors()));
        QVERIFY(waitForGaming(settings.gaming()));
        auto *prefixField = page.object(QStringLiteral("compatibilityPrefixField"));
        QVERIFY(prefixField);
        settings.gaming()->setCompatibilityOption(QStringLiteral("extra_env"),
                                                   QStringLiteral("\"WINEDEBUG=-all\""));
        QTRY_VERIFY_WITH_TIMEOUT(settings.gaming()->stateJson().contains(QStringLiteral("WINEDEBUG=-all")),
                                 7000);
        QTRY_VERIFY_WITH_TIMEOUT(!settings.gaming()->busy(), 7000);
        const QString path = QDir(environment.configHome())
                                 .filePath(QStringLiteral("AstreaOS/gaming/compatibility.json"));
        QCOMPARE(readJsonObject(path).value(QStringLiteral("extra_env")).toString(),
                 QStringLiteral("WINEDEBUG=-all"));
    }

    {
        SettingsPage recreated(settings, compatibilityUrl);
        QVERIFY2(recreated.isValid(), qPrintable(recreated.errors()));
        QVERIFY(waitForGaming(settings.gaming()));
        auto *environmentField = recreated.object(QStringLiteral("compatibilityEnvironmentField"));
        QVERIFY(environmentField);
        QTRY_COMPARE_WITH_TIMEOUT(environmentField->property("text").toString(),
                                  QStringLiteral("WINEDEBUG=-all"), 7000);
        QVERIFY2(recreated.warnings().isEmpty(),
                 qPrintable(recreated.warnings().join(QLatin1Char('\n'))));
    }

    {
        SettingsPage recreated(settings, protonUrl);
        QVERIFY2(recreated.isValid(), qPrintable(recreated.errors()));
        QVERIFY(waitForGaming(settings.gaming()));
        auto *widthField = recreated.object(QStringLiteral("gamescopeWidthField"));
        QVERIFY(widthField);
        QTRY_COMPARE_WITH_TIMEOUT(widthField->property("text").toString(), QStringLiteral("2560"),
                                  7000);
        QVERIFY(!recreated.object(QStringLiteral("protonPreviewNotice")));
    }
    SettingsController reloaded;
    QVERIFY(waitForGaming(reloaded.gaming()));
    QVERIFY(reloaded.gaming()->stateJson().contains(QStringLiteral("WINEDEBUG=-all")));
    QVERIFY(reloaded.gaming()->stateJson().contains(QStringLiteral("2560")));
}

void SettingsGamingQmlTest::performancePreferenceAndExplicitApplyUseNativeBackend()
{
    GamingTestEnvironment environment;
    QVERIFY(environment.isValid());
    QVERIFY(environment.addHelpers());
    SettingsController settings;
    settings.performance()->refresh();
    QVERIFY(waitForPerformance(settings.performance()));

    settings.performance()->setPreference(QStringLiteral("prefer_gamemode"), QStringLiteral("false"));
    QTRY_VERIFY_WITH_TIMEOUT(settings.performance()->stateJson().contains(
                                 QStringLiteral("\"prefer_gamemode\":false")), 7000);
    QTRY_VERIFY_WITH_TIMEOUT(!settings.performance()->busy(), 7000);
    const QString performancePath = QDir(environment.configHome())
                                       .filePath(QStringLiteral("AstreaOS/system/performance.json"));
    QCOMPARE(readJsonObject(performancePath).value(QStringLiteral("prefer_gamemode")).toBool(), false);

    settings.performance()->applyProfile(QStringLiteral("performance"));
    QTRY_VERIFY_WITH_TIMEOUT(settings.performance()->stateJson().contains(
                                 QStringLiteral("\"profile\":\"performance\"")), 7000);
    QTRY_VERIFY_WITH_TIMEOUT(!settings.performance()->busy(), 7000);
    QCOMPARE(readJsonObject(performancePath).value(QStringLiteral("profile")).toString(),
             QStringLiteral("performance"));
    QFile calls(environment.callsPath());
    QVERIFY(calls.open(QIODevice::ReadOnly));
    QVERIFY(QString::fromUtf8(calls.readAll()).contains(QStringLiteral("powerprofilesctl set performance")));
}

void SettingsGamingQmlTest::missingHelpersDisableSystemActions()
{
    GamingTestEnvironment environment;
    QVERIFY(environment.isValid());
    SettingsController settings;
    settings.performance()->refresh();
    QVERIFY(waitForPerformance(settings.performance()));
    SettingsPage page(settings, QUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Settings/qml/pages/system/Performance.qml")));
    QVERIFY2(page.isValid(), qPrintable(page.errors()));
    QVERIFY(waitForPerformance(settings.performance()));
    auto *profile = page.object(QStringLiteral("powerProfileSelect"));
    auto *scheduler = page.object(QStringLiteral("applySchedulerButton"));
    QVERIFY(profile);
    QVERIFY(scheduler);
    QVERIFY(!profile->property("enabled").toBool());
    QVERIFY(!scheduler->property("enabled").toBool());
    QVERIFY(!page.object(QStringLiteral("performancePreviewNotice")));
    QVERIFY2(page.warnings().isEmpty(), qPrintable(page.warnings().join(QLatin1Char('\n'))));
}

void SettingsGamingQmlTest::failedApplyAndInvalidInputRemainVisibleWithoutFalseSuccess()
{
    GamingTestEnvironment environment;
    QVERIFY(environment.isValid());
    QVERIFY(environment.addHelpers());
    environment.setFailure(true);
    SettingsController settings;
    settings.performance()->refresh();
    QVERIFY(waitForPerformance(settings.performance()));
    SettingsPage page(settings, QUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Settings/qml/pages/system/Performance.qml")));
    QVERIFY2(page.isValid(), qPrintable(page.errors()));
    QVERIFY(waitForPerformance(settings.performance()));
    settings.performance()->applyProfile(QStringLiteral("performance"));
    QTRY_VERIFY_WITH_TIMEOUT(!settings.performance()->lastError().isEmpty(), 7000);
    QTRY_VERIFY_WITH_TIMEOUT(!settings.performance()->busy(), 7000);
    auto *feedback = page.object(QStringLiteral("performanceBackendStatus"));
    QVERIFY(feedback);
    QVERIFY(feedback->property("visible").toBool());
    QVERIFY2(page.warnings().isEmpty(), qPrintable(page.warnings().join(QLatin1Char('\n'))));
    const QString performancePath = QDir(environment.configHome())
                                       .filePath(QStringLiteral("AstreaOS/system/performance.json"));
    QVERIFY(!QFileInfo::exists(performancePath));

    SettingsPage protonPage(settings, QUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Settings/qml/pages/gaming/Proton.qml")));
    QVERIFY2(protonPage.isValid(), qPrintable(protonPage.errors()));
    QVERIFY(waitForGaming(settings.gaming()));
    settings.gaming()->setProtonOption(QStringLiteral("gamescope_width"), QStringLiteral("20"));
    QTRY_VERIFY_WITH_TIMEOUT(settings.gaming()->lastError().contains(QStringLiteral("gamescope_width")),
                             7000);
    QTRY_VERIFY_WITH_TIMEOUT(!settings.gaming()->busy(), 7000);
    auto *gamingFeedback = protonPage.object(QStringLiteral("protonBackendStatus"));
    auto *retry = protonPage.object(QStringLiteral("protonRetryRefreshButton"));
    QVERIFY(gamingFeedback);
    QVERIFY(retry);
    QVERIFY(gamingFeedback->property("visible").toBool());
    QVERIFY(retry->property("visible").toBool());
    QVERIFY2(protonPage.warnings().isEmpty(),
             qPrintable(protonPage.warnings().join(QLatin1Char('\n'))));
    const QString protonPath = QDir(environment.configHome())
                                   .filePath(QStringLiteral("AstreaOS/gaming/proton.json"));
    QVERIFY(!QFileInfo::exists(protonPath));
}

QTEST_MAIN(SettingsGamingQmlTest)
#include "SettingsGamingQmlTest.moc"
