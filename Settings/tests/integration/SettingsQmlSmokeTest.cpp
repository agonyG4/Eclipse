#include "core/SettingsController.hpp"
#include "services/i18n/SettingsTranslationController.hpp"
#include "theme/ThemeController.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QTemporaryDir>
#include <QtTest>

class SettingsQmlSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsCompositorRouteOffscreen();
};

void SettingsQmlSmokeTest::loadsCompositorRouteOffscreen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController(directory.filePath(QStringLiteral("missing-theme.json")));
    QQmlApplicationEngine engine;
    QList<QQmlError> qmlWarnings;

    connect(&engine, &QQmlApplicationEngine::warnings, this,
            [&qmlWarnings](const QList<QQmlError> &warnings) {
                qmlWarnings.append(warnings);
            });

    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();
    QVERIFY(settingsController.selectSection(QStringLiteral("compositor")));
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("compositorPage")) != nullptr, 1000);
    QObject *page = root->findChild<QObject *>(QStringLiteral("compositorPage"));
    QVERIFY(page != nullptr);
    QCOMPARE(page->property("animationsEnabled").toBool(), true);
    page->setProperty("animationsEnabled", false);
    QCOMPARE(page->property("animationsEnabled").toBool(), false);

    QVERIFY(settingsController.selectSection(QStringLiteral("system")));
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("compositorPage")) == nullptr, 1000);
    QVERIFY(settingsController.selectSection(QStringLiteral("compositor")));
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("compositorPage")) != nullptr, 1000);
    QCOMPARE(root->findChild<QObject *>(QStringLiteral("compositorPage"))->property("animationsEnabled").toBool(), true);
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(qmlWarnings.isEmpty() ? QString() : qmlWarnings.constFirst().toString()));
}

QTEST_MAIN(SettingsQmlSmokeTest)
#include "SettingsQmlSmokeTest.moc"
