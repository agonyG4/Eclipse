#include "services/i18n/SettingsTranslationController.hpp"
#include "theme/ThemeController.hpp"

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

class SettingsComponentSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsRepresentativeRegisteredComponents();
    void sliderThumbPressAtMinimumDoesNotJump();
    void sliderThumbPressAtMaximumDoesNotJump();
};

namespace {

struct SliderFixture {
    QTemporaryDir directory;
    SettingsTranslationController translationController;
    ThemeController themeController;
    QQmlApplicationEngine engine;
    QQuickWindow window;
    QQuickItem *slider = nullptr;

    SliderFixture()
        : themeController(directory.filePath(QStringLiteral("missing-theme.json")))
    {
        engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
        engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    }

    bool create(qreal initialValue, QString *error)
    {
        const QByteArray fixture = R"qml(
import QtQuick
import Astrea.Settings 1.0 as Settings

Settings.Slider {
    objectName: "sliderRegressionControl"
    width: 216
    height: 32
    from: 0
    to: 10
    stepSize: 1
}
)qml";

        QQmlComponent component(&engine);
        component.setData(fixture, QUrl(QStringLiteral("qrc:/settings-slider-regression.qml")));
        if (component.status() != QQmlComponent::Ready) {
            *error = component.errorString();
            return false;
        }

        slider = qobject_cast<QQuickItem *>(component.create());
        if (!slider) {
            *error = QStringLiteral("failed to create slider regression fixture");
            return false;
        }
        slider->setProperty("value", initialValue);
        slider->setParentItem(window.contentItem());
        window.resize(216, 32);
        window.show();
        QTest::qWait(20);
        return true;
    }

    QPoint renderedHandleCenter() const
    {
        const auto *handle = qobject_cast<QQuickItem *>(slider->property("handle").value<QObject *>());
        Q_ASSERT(handle != nullptr);
        return handle->mapToItem(window.contentItem(),
                                 QPointF(handle->width() / 2.0, handle->height() / 2.0))
            .toPoint();
    }
};

} // namespace

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

    Settings.Slider {
        objectName: "componentSmokeSlider"
        from: 0
        to: 10
        stepSize: 2
        value: 4
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
    QObject *slider = root->findChild<QObject *>(QStringLiteral("componentSmokeSlider"));
    QVERIFY(slider != nullptr);
    QCOMPARE(slider->property("from").toReal(), 0.0);
    QCOMPARE(slider->property("to").toReal(), 10.0);
    QCOMPARE(slider->property("stepSize").toReal(), 2.0);
    QCOMPARE(slider->property("value").toReal(), 4.0);
    QVERIFY(QMetaObject::invokeMethod(slider, "increase"));
    QCOMPARE(slider->property("value").toReal(), 6.0);
    QVERIFY2(qmlWarnings.isEmpty(),
             qPrintable(qmlWarnings.isEmpty() ? QString() : qmlWarnings.constFirst().toString()));
    delete root;
}

void SettingsComponentSmokeTest::sliderThumbPressAtMinimumDoesNotJump()
{
    SliderFixture fixture;
    QVERIFY(fixture.directory.isValid());

    QString error;
    QVERIFY2(fixture.create(0, &error), qPrintable(error));
    QVERIFY(fixture.slider != nullptr);
    QCOMPARE(fixture.slider->property("value").toReal(), 0.0);

    QTest::mouseClick(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                      fixture.renderedHandleCenter());
    QCoreApplication::processEvents();

    QCOMPARE(fixture.slider->property("value").toReal(), 0.0);
}

void SettingsComponentSmokeTest::sliderThumbPressAtMaximumDoesNotJump()
{
    SliderFixture fixture;
    QVERIFY(fixture.directory.isValid());

    QString error;
    QVERIFY2(fixture.create(10, &error), qPrintable(error));
    QVERIFY(fixture.slider != nullptr);
    QCOMPARE(fixture.slider->property("value").toReal(), 10.0);

    QTest::mouseClick(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                      fixture.renderedHandleCenter());
    QCoreApplication::processEvents();

    QCOMPARE(fixture.slider->property("value").toReal(), 10.0);
}

QTEST_MAIN(SettingsComponentSmokeTest)
#include "SettingsComponentSmokeTest.moc"
