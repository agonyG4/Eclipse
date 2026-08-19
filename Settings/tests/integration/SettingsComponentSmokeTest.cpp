#include "services/i18n/SettingsTranslationController.hpp"
#include "theme/ThemeController.hpp"

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlError>
#include <QTemporaryDir>
#include <QtTest>

class SettingsComponentSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsRepresentativeRegisteredComponents();
};

void SettingsComponentSmokeTest::loadsRepresentativeRegisteredComponents()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    SettingsTranslationController translationController;
    ThemeController themeController(directory.filePath(QStringLiteral("missing-theme.json")));
    QQmlApplicationEngine engine;
    QList<QQmlError> qmlWarnings;
    connect(&engine, &QQmlApplicationEngine::warnings, this,
            [&qmlWarnings](const QList<QQmlError> &warnings) {
                qmlWarnings.append(warnings);
            });
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);

    const QByteArray fixture = R"qml(
import QtQuick
import Astrea.Settings 1.0 as Settings

Item {
    width: 900
    height: 700

    Settings.FormCard {
        width: 400
        height: 120
        Settings.SettingRow {
            label: "Toggle"
            isLast: true
            Settings.ToggleSwitch {}
        }
    }

    Settings.NavItem {
        width: 240
        height: 40
        label: "System"
        selected: false
    }

    Settings.DnsPresetChip { label: "Automatic" }
    Settings.DnsStatusCard { width: 320; providerLabel: "Automatic (ISP)" }
    Settings.ProgressCard { x: 420; width: 320; title: "Progress" }
    Settings.SpeedCard { x: 420; y: 100; width: 320; label: "Download"; history: [1, 2, 3] }
    Settings.StatusDot { x: 10; y: 160; active: true }

    Settings.DisplayLabel { y: 190; text: "Typography" }

    Settings.ContextMenu {
        Settings.ContextMenuAction { label: "Action" }
        Settings.ContextMenuDivider {}
    }
}
)qml";

    QQmlComponent component(&engine);
    component.setData(fixture, QUrl(QStringLiteral("qrc:/settings-component-fixture.qml")));
    if (component.status() != QQmlComponent::Ready) {
        const QStringList errors = component.errors().isEmpty()
            ? QStringList{QStringLiteral("unknown QML component error")}
            : QStringList{component.errors().constFirst().toString()};
        QFAIL(qPrintable(errors.constFirst()));
    }

    QObject *root = component.create();
    QVERIFY(root != nullptr);
    QVERIFY2(qmlWarnings.isEmpty(),
             qPrintable(qmlWarnings.isEmpty() ? QString() : qmlWarnings.constFirst().toString()));
    delete root;
}

QTEST_MAIN(SettingsComponentSmokeTest)
#include "SettingsComponentSmokeTest.moc"
