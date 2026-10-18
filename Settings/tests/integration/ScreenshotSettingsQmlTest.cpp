#include "core/SettingsController.hpp"
#include "services/i18n/SettingsTranslationController.hpp"
#include "theme/ThemeController.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <memory>

class ScreenshotSettingsQmlTest final : public QObject {
    Q_OBJECT

private slots:
    void routeLoadsDesktopPreviewControlsAndReportsPersistenceStatus();
    void controlsPersistWithoutEmbeddedPreview();
    void replayControlsAreDelegatedToShell();
};

namespace {

class ConfigHomeScope {
public:
    explicit ConfigHomeScope(const QString &path)
        : m_previous(qgetenv("XDG_CONFIG_HOME"))
    {
        qputenv("XDG_CONFIG_HOME", path.toUtf8());
    }

    ~ConfigHomeScope()
    {
        if (m_previous.isNull())
            qunsetenv("XDG_CONFIG_HOME");
        else
            qputenv("XDG_CONFIG_HOME", m_previous);
    }

private:
    QByteArray m_previous;
};

struct ScreenshotSettingsPage {
    SettingsController settings;
    SettingsTranslationController translations;
    ThemeController theme;
    QQmlApplicationEngine engine;
    QQuickWindow window;
    std::unique_ptr<QObject> page;

    ScreenshotSettingsPage()
    {
        engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settings);
        engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translations);
        engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &theme);
        QQmlComponent component(&engine, QUrl(QStringLiteral(
            "qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Screenshots.qml")));
        if (component.status() != QQmlComponent::Ready)
            qFatal("Screenshot Settings page failed to load: %s",
                   qPrintable(component.errorString()));
        page.reset(component.create());
        if (!page)
            qFatal("Screenshot Settings page object failed to create: %s",
                   qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(page.get());
        if (!item)
            qFatal("Screenshot Settings page root is not a QQuickItem");
        window.resize(960, 800);
        item->setWidth(window.width());
        item->setHeight(window.height());
        item->setParentItem(window.contentItem());
        window.show();
    }

    QObject *object(const QString &name) const
    {
        return page ? page->findChild<QObject *>(name) : nullptr;
    }
};

} // namespace

void ScreenshotSettingsQmlTest::routeLoadsDesktopPreviewControlsAndReportsPersistenceStatus()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    ConfigHomeScope configScope(configHome.path());
    ScreenshotSettingsPage fixture;

    QVERIFY(fixture.settings.navigateTo(QStringLiteral("customization")));
    QVERIFY(fixture.settings.currentDestinationChildren().size() == 7);
    QVERIFY(fixture.settings.navigateTo(QStringLiteral("screenshots")));
    QCOMPARE(fixture.settings.selectedSidebarId(), QStringLiteral("customization"));
    QCOMPARE(fixture.page->objectName(), QStringLiteral("screenshotsPage"));
    QVERIFY(!fixture.object(QStringLiteral("screenshotMockVisual")));
    auto *cornerSlider = fixture.object(QStringLiteral("screenshotCornerRadiusSlider"));
    QVERIFY(cornerSlider);
    QVERIFY(cornerSlider->property("detentEnabled").toBool());
    QCOMPARE(cornerSlider->property("detentValue").toInt(), 10);
    QVERIFY(!fixture.object(QStringLiteral("screenshotDesktopPreview")));
    QVERIFY(fixture.object(QStringLiteral("screenshotAnimationReplay")));
    QCOMPARE(fixture.settings.screenshots()->status(), QStringLiteral("ready"));
    QVERIFY(fixture.settings.screenshots()->filePath().startsWith(configHome.path()));
}

void ScreenshotSettingsQmlTest::controlsPersistWithoutEmbeddedPreview()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    ConfigHomeScope configScope(configHome.path());
    ScreenshotSettingsPage fixture;
    QVERIFY(!fixture.object(QStringLiteral("screenshotMockVisual")));
    QCOMPARE(fixture.settings.screenshots()->cornerRadius(), 10);
    fixture.settings.screenshots()->setCornerRadius(24);
    fixture.settings.screenshots()->setBorderEnabled(false);
    fixture.settings.screenshots()->setBorderWidth(3);
    fixture.settings.screenshots()->setShadowEnabled(false);
    fixture.settings.screenshots()->setShadowStrength(0.8);
    fixture.settings.screenshots()->setThumbnailWidth(360);
    fixture.settings.screenshots()->setThumbnailHeight(200);
    fixture.settings.screenshots()->setPosition(QStringLiteral("top-left"));
    fixture.settings.screenshots()->setExitAnimation(QStringLiteral("fade"));
    fixture.settings.screenshots()->setExitDurationMs(300);

    QVERIFY(fixture.settings.screenshots()->flush());
    QFile configurationFile(fixture.settings.screenshots()->filePath());
    QVERIFY(configurationFile.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(configurationFile.readAll());
    QVERIFY(document.isObject());
    QCOMPARE(document.object().value(QStringLiteral("cornerRadius")).toInt(), 24);
    QCOMPARE(document.object().value(QStringLiteral("borderEnabled")).toBool(), false);
    QCOMPARE(document.object().value(QStringLiteral("position")).toString(),
             QStringLiteral("top-left"));
    QCOMPARE(document.object().value(QStringLiteral("exitAnimation")).toString(),
             QStringLiteral("fade"));
}

void ScreenshotSettingsQmlTest::replayControlsAreDelegatedToShell()
{
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    ConfigHomeScope configScope(configHome.path());
    ScreenshotSettingsPage fixture;
    fixture.settings.screenshots()->setEntryAnimation(QStringLiteral("scale-fade"));
    fixture.settings.screenshots()->setExitAnimation(QStringLiteral("slide-fade"));
    fixture.settings.screenshots()->setEntryDurationMs(120);
    fixture.settings.screenshots()->setExitDurationMs(120);

    QVERIFY(fixture.object(QStringLiteral("screenshotAnimationReplay")));
    QVERIFY(!fixture.object(QStringLiteral("screenshotMockThumbnail")));
    QVERIFY(!fixture.settings.screenshotDemoCommand(QStringLiteral("invalid")));
    QCOMPARE(fixture.settings.screenshots()->entryAnimation(), QStringLiteral("scale-fade"));
    QCOMPARE(fixture.settings.screenshots()->exitAnimation(), QStringLiteral("slide-fade"));
}

QTEST_MAIN(ScreenshotSettingsQmlTest)

#include "ScreenshotSettingsQmlTest.moc"
