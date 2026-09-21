#include "core/SettingsController.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>

namespace {

struct PreviousEnvironmentValue {
    QByteArray name;
    QByteArray value;
    bool wasSet = false;
};

void restoreEnvironment(const PreviousEnvironmentValue &previous)
{
    if (previous.wasSet)
        qputenv(previous.name.constData(), previous.value);
    else
        qunsetenv(previous.name.constData());
}

class ThemeTestEnvironment final {
public:
    ThemeTestEnvironment()
        : m_home(previousValue("HOME"))
        , m_dataHome(previousValue("XDG_DATA_HOME"))
        , m_dataDirs(previousValue("XDG_DATA_DIRS"))
    {
        if (!m_directory.isValid())
            return;
        const QByteArray home = m_directory.path().toLocal8Bit();
        qputenv("HOME", home);
        qputenv("XDG_DATA_HOME", QDir(m_directory.path()).filePath("data").toLocal8Bit());
        qputenv("XDG_DATA_DIRS", QDir(m_directory.path()).filePath("system").toLocal8Bit());
    }

    ~ThemeTestEnvironment()
    {
        restoreEnvironment(m_home);
        restoreEnvironment(m_dataHome);
        restoreEnvironment(m_dataDirs);
    }

    bool isValid() const { return m_directory.isValid(); }
    QString home() const { return m_directory.path(); }
    QString configPath() const
    {
        return QDir(home()).filePath(".config/AstreaOS/ui/theme.json");
    }

    bool createThemes() const
    {
        for (const QString &id : {QStringLiteral("theme-a"),
                                  QStringLiteral("theme-b"),
                                  QStringLiteral("theme-c")}) {
            const QString themeDirectory = QDir(home()).filePath(".icons/" + id);
            if (!QDir().mkpath(themeDirectory))
                return false;
            QFile index(QDir(themeDirectory).filePath("index.theme"));
            if (!index.open(QIODevice::WriteOnly | QIODevice::Truncate))
                return false;
            const QByteArray metadata =
                "[Icon Theme]\nName=" + id.toUtf8()
                + "\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n";
            if (index.write(metadata) != metadata.size())
                return false;
        }
        return true;
    }

private:
    static PreviousEnvironmentValue previousValue(const char *name)
    {
        return {QByteArray(name), qgetenv(name), qEnvironmentVariableIsSet(name)};
    }

    QTemporaryDir m_directory;
    PreviousEnvironmentValue m_home;
    PreviousEnvironmentValue m_dataHome;
    PreviousEnvironmentValue m_dataDirs;
};

bool hasTheme(QObject *controller, const QString &themeId)
{
    const QVariantList themes = controller->property("themes").toList();
    for (const QVariant &theme : themes) {
        if (theme.toMap().value(QStringLiteral("id")).toString() == themeId)
            return true;
    }
    return false;
}

bool invokeSetIconTheme(QObject *controller, const QString &themeId)
{
    return QMetaObject::invokeMethod(controller,
                                     "setIconTheme",
                                     Qt::DirectConnection,
                                     Q_ARG(QString, themeId));
}

bool createThemeConfigFifo(const QString &path)
{
    const QString parent = QFileInfo(path).path();
    if (!QDir().mkpath(parent))
        return false;
    const QByteArray encodedPath = QFile::encodeName(path);
    return ::mkfifo(encodedPath.constData(), 0600) == 0;
}

class FifoGateWriter final {
public:
    FifoGateWriter(const QString &path, const std::atomic_bool *timerFired = nullptr)
        : m_timerFired(timerFired)
        , m_thread([this, encodedPath = QFile::encodeName(path).toStdString()] {
            const auto openDeadline = std::chrono::steady_clock::now()
                                      + std::chrono::seconds(3);
            int descriptor = -1;
            while (std::chrono::steady_clock::now() < openDeadline) {
                descriptor = ::open(encodedPath.c_str(), O_WRONLY | O_NONBLOCK);
                if (descriptor >= 0)
                    break;
                if (errno != ENXIO && errno != EINTR)
                    return;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (descriptor < 0)
                return;

            m_readerConnected.store(true);
            const auto releaseDeadline = std::chrono::steady_clock::now()
                                         + std::chrono::seconds(2);
            while (!m_release.load() && std::chrono::steady_clock::now() < releaseDeadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (m_timerFired)
                m_timerWasFired.store(m_timerFired->load());
            static constexpr char jsonObject[] = "{}";
            static_cast<void>(::write(descriptor, jsonObject, sizeof(jsonObject) - 1));
            static_cast<void>(::close(descriptor));
        })
    {
    }

    ~FifoGateWriter()
    {
        release();
        join();
    }

    bool readerConnected() const { return m_readerConnected.load(); }
    bool timerWasFired() const { return m_timerWasFired.load(); }
    void release() { m_release.store(true); }
    void join()
    {
        if (m_thread.joinable())
            m_thread.join();
    }

private:
    const std::atomic_bool *m_timerFired;
    std::atomic_bool m_readerConnected = false;
    std::atomic_bool m_release = false;
    std::atomic_bool m_timerWasFired = false;
    std::thread m_thread;
};

} // namespace

class SettingsThemesControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void exposesThemesControllerThroughSettingsController();
    void selectionCompletesOffTheQtCallerThreadAndSystemDefaultPersists();
    void staleSelectionCompletionCannotReplaceTheNewestRequest();
    void persistenceFailureRollsBackProjectedSelection();
    void destroyingControllerDuringPersistenceDoesNotWaitForWorker();
};

void SettingsThemesControllerTest::exposesThemesControllerThroughSettingsController()
{
    ThemeTestEnvironment environment;
    QVERIFY(environment.isValid());

    SettingsController controller;
    QObject *themes = controller.themes();
    QVERIFY(themes != nullptr);
    QVERIFY(controller.property("themes").value<QObject *>() == themes);

    const QMetaObject *metaObject = themes->metaObject();
    for (const char *name : {"themes", "selectedIconTheme", "busy", "refreshing", "lastError"})
        QVERIFY(metaObject->indexOfProperty(name) >= 0);
    for (const char *method : {"refresh()", "setIconTheme(QString)", "useSystemDefault()"})
        QVERIFY(metaObject->indexOfMethod(method) >= 0);
    for (const char *signal : {"themesChanged()",
                               "selectedIconThemeChanged()",
                               "busyChanged()",
                               "refreshingChanged()",
                               "errorChanged()"}) {
        QVERIFY(metaObject->indexOfSignal(signal) >= 0);
    }
}

void SettingsThemesControllerTest::selectionCompletesOffTheQtCallerThreadAndSystemDefaultPersists()
{
    ThemeTestEnvironment environment;
    QVERIFY(environment.isValid());
    QVERIFY(environment.createThemes());
    SettingsThemesController themes;
    QTRY_VERIFY_WITH_TIMEOUT(hasTheme(&themes, QStringLiteral("theme-c")), 5000);

    const QString configPath = environment.configPath();
    QVERIFY(createThemeConfigFifo(configPath));
    std::atomic_bool timerFired = false;
    QSignalSpy selectedThemeSpy(&themes, SIGNAL(selectedIconThemeChanged()));
    QVERIFY(selectedThemeSpy.isValid());
    FifoGateWriter writer(configPath, &timerFired);
    QTimer::singleShot(0, [&timerFired] { timerFired.store(true); });

    QVERIFY(invokeSetIconTheme(&themes, QStringLiteral("theme-c")));
    QTRY_VERIFY_WITH_TIMEOUT(writer.readerConnected(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(timerFired.load(), 1000);
    QVERIFY(themes.property("busy").toBool());
    QVERIFY(!themes.property("refreshing").toBool());
    QCOMPARE(themes.property("selectedIconTheme").toString(), QStringLiteral("theme-c"));
    QCOMPARE(selectedThemeSpy.count(), 1);
    writer.release();
    writer.join();
    QVERIFY(writer.timerWasFired());
    QTRY_VERIFY_WITH_TIMEOUT(!themes.property("busy").toBool(), 5000);
    QCOMPARE(themes.property("selectedIconTheme").toString(), QStringLiteral("theme-c"));
    QVERIFY(themes.property("lastError").toString().isEmpty());
    QCOMPARE(selectedThemeSpy.count(), 1);

    QVERIFY(QFile::remove(configPath));
    QVERIFY(createThemeConfigFifo(configPath));
    FifoGateWriter defaultWriter(configPath);
    QVERIFY(QMetaObject::invokeMethod(&themes, "useSystemDefault", Qt::DirectConnection));
    QTRY_VERIFY_WITH_TIMEOUT(defaultWriter.readerConnected(), 3000);
    QVERIFY(themes.property("busy").toBool());
    QVERIFY(!themes.property("refreshing").toBool());
    QVERIFY(themes.property("selectedIconTheme").toString().isEmpty());
    QCOMPARE(selectedThemeSpy.count(), 2);
    defaultWriter.release();
    defaultWriter.join();
    QTRY_VERIFY_WITH_TIMEOUT(!themes.property("busy").toBool(), 5000);
    QVERIFY(themes.property("selectedIconTheme").toString().isEmpty());
    QCOMPARE(selectedThemeSpy.count(), 2);

    QVERIFY(QMetaObject::invokeMethod(&themes, "useSystemDefault", Qt::DirectConnection));
    QVERIFY(!themes.property("busy").toBool());
    QCOMPARE(selectedThemeSpy.count(), 2);
    QFile config(configPath);
    QVERIFY(config.open(QIODevice::ReadOnly));
    const QJsonObject persisted = QJsonDocument::fromJson(config.readAll()).object();
    QVERIFY(!persisted.contains(QStringLiteral("system_icon_theme")));
}

void SettingsThemesControllerTest::staleSelectionCompletionCannotReplaceTheNewestRequest()
{
    ThemeTestEnvironment environment;
    QVERIFY(environment.isValid());
    QVERIFY(environment.createThemes());
    SettingsThemesController themes;
    QTRY_VERIFY_WITH_TIMEOUT(hasTheme(&themes, QStringLiteral("theme-c")), 5000);

    const QString configPath = environment.configPath();
    QVERIFY(createThemeConfigFifo(configPath));
    QSignalSpy selectedThemeSpy(&themes, SIGNAL(selectedIconThemeChanged()));
    QVERIFY(selectedThemeSpy.isValid());
    FifoGateWriter writer(configPath);

    QVERIFY(invokeSetIconTheme(&themes, QStringLiteral("theme-a")));
    QTRY_VERIFY_WITH_TIMEOUT(writer.readerConnected(), 3000);
    QCOMPARE(themes.property("selectedIconTheme").toString(), QStringLiteral("theme-a"));
    QCOMPARE(selectedThemeSpy.count(), 1);
    QVERIFY(invokeSetIconTheme(&themes, QStringLiteral("theme-b")));
    QCOMPARE(themes.property("selectedIconTheme").toString(), QStringLiteral("theme-b"));
    QCOMPARE(selectedThemeSpy.count(), 2);
    QVERIFY(invokeSetIconTheme(&themes, QStringLiteral("theme-c")));
    QCOMPARE(themes.property("selectedIconTheme").toString(), QStringLiteral("theme-c"));
    QCOMPARE(selectedThemeSpy.count(), 3);
    QVERIFY(themes.property("busy").toBool());
    writer.release();
    writer.join();

    QTRY_VERIFY_WITH_TIMEOUT(!themes.property("busy").toBool(), 5000);
    QCOMPARE(themes.property("selectedIconTheme").toString(), QStringLiteral("theme-c"));
    QCOMPARE(selectedThemeSpy.count(), 3);
    QFile config(configPath);
    QVERIFY(config.open(QIODevice::ReadOnly));
    const QJsonObject persisted = QJsonDocument::fromJson(config.readAll()).object();
    QCOMPARE(persisted.value(QStringLiteral("system_icon_theme")).toString(),
             QStringLiteral("theme-c"));
}

void SettingsThemesControllerTest::persistenceFailureRollsBackProjectedSelection()
{
    ThemeTestEnvironment environment;
    QVERIFY(environment.isValid());
    QVERIFY(environment.createThemes());
    SettingsThemesController themes;
    QTRY_VERIFY_WITH_TIMEOUT(hasTheme(&themes, QStringLiteral("theme-c")), 5000);

    const QString configDirectory = QDir(environment.home()).filePath(QStringLiteral(".config/AstreaOS"));
    QVERIFY(QDir().mkpath(QFileInfo(configDirectory).path()));
    QFile blocker(configDirectory);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    QVERIFY(blocker.write("not-a-directory") > 0);
    blocker.close();

    QSignalSpy selectedThemeSpy(&themes, SIGNAL(selectedIconThemeChanged()));
    QVERIFY(selectedThemeSpy.isValid());
    QVERIFY(invokeSetIconTheme(&themes, QStringLiteral("theme-c")));
    QCOMPARE(themes.property("selectedIconTheme").toString(), QStringLiteral("theme-c"));
    QCOMPARE(selectedThemeSpy.count(), 1);

    QTRY_VERIFY_WITH_TIMEOUT(!themes.property("busy").toBool(), 5000);
    QVERIFY(themes.property("selectedIconTheme").toString().isEmpty());
    QCOMPARE(selectedThemeSpy.count(), 2);
    QVERIFY(!themes.property("lastError").toString().isEmpty());
}

void SettingsThemesControllerTest::destroyingControllerDuringPersistenceDoesNotWaitForWorker()
{
    ThemeTestEnvironment environment;
    QVERIFY(environment.isValid());
    QVERIFY(environment.createThemes());
    auto *themes = new SettingsThemesController;
    QTRY_VERIFY_WITH_TIMEOUT(hasTheme(themes, QStringLiteral("theme-a")), 5000);

    const QString configPath = environment.configPath();
    QVERIFY(createThemeConfigFifo(configPath));
    FifoGateWriter writer(configPath);
    QVERIFY(invokeSetIconTheme(themes, QStringLiteral("theme-a")));
    QTRY_VERIFY_WITH_TIMEOUT(writer.readerConnected(), 3000);

    QElapsedTimer destructionTimer;
    destructionTimer.start();
    delete themes;
    const qint64 destructionDuration = destructionTimer.elapsed();
    writer.release();
    writer.join();

    QVERIFY2(destructionDuration < 100,
             "destroying the Qt controller waited for the blocked persistence work");
    QCoreApplication::processEvents();
}

QTEST_GUILESS_MAIN(SettingsThemesControllerTest)
#include "SettingsThemesControllerTest.moc"
