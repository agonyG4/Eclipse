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
    void sliderWithoutDetentRemainsContinuous();
    void sliderEntersDefaultDetent();
    void sliderDetentUsesHysteresis();
    void sliderDetentPulseIsOneShotPerEntry();
    void sliderControlledBindingSurvivesDetent();
    void sliderControlledPressWithoutMoveDoesNotJump();
    void sliderControlledKeyboardEditStartsFromModelValue();
    void sliderControlledModelChangeDuringDragDefersNativeSync();
    void sliderLatchClearsOnReleaseAndRearmsSnap();
    void sliderKeyboardCrossesDefaultDetent();
    void sliderMarkerUsesMirroredTrackGeometry();
};

namespace {

struct SliderFixture {
    QTemporaryDir directory;
    SettingsTranslationController translationController;
    ThemeController themeController;
    QQmlApplicationEngine engine;
    QQuickWindow window;
    QObject *fixtureRoot = nullptr;
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
    stepSize: 0.1
    property bool mirrorForTest: false
    LayoutMirroring.enabled: mirrorForTest
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

    bool createControlled(qreal initialValue, QString *error)
    {
        const QByteArray fixture = R"qml(
import QtQuick
import Astrea.Settings 1.0 as Settings

Item {
    objectName: "controlledSliderFixture"
    width: 216
    height: 32
    property real backendValue: 2
    property bool controlledMode: false

    Settings.Slider {
        objectName: "sliderRegressionControl"
        anchors.fill: parent
        from: 0
        to: 10
        stepSize: 0.1
        value: 0
        modelValueEnabled: parent.controlledMode
        modelValue: parent.backendValue
        detentEnabled: true
        detentValue: 5
        valueText: Number(displayedValue).toFixed(1)
        onValueEdited: editedValue => parent.backendValue = editedValue
    }
}
)qml";

        QQmlComponent component(&engine);
        component.setData(fixture, QUrl(QStringLiteral("qrc:/settings-controlled-slider-regression.qml")));
        if (component.status() != QQmlComponent::Ready) {
            *error = component.errorString();
            return false;
        }

        fixtureRoot = component.create();
        auto *rootItem = qobject_cast<QQuickItem *>(fixtureRoot);
        if (!rootItem) {
            *error = QStringLiteral("failed to create controlled slider fixture");
            return false;
        }
        rootItem->setProperty("backendValue", initialValue);
        rootItem->setParentItem(window.contentItem());
        rootItem->setProperty("controlledMode", true);
        slider = rootItem->findChild<QQuickItem *>(QStringLiteral("sliderRegressionControl"));
        if (!slider) {
            *error = QStringLiteral("failed to find controlled slider");
            return false;
        }
        window.resize(216, 32);
        window.show();
        QTest::qWait(20);
        return true;
    }

    QPoint pointForVisualPosition(qreal position) const
    {
        const qreal trackStart = slider->property("trackStart").toReal();
        const qreal trackWidth = slider->property("trackWidth").toReal();
        return slider->mapToItem(window.contentItem(),
                                 QPointF(trackStart + position * trackWidth,
                                         slider->property("height").toReal() / 2.0))
            .toPoint();
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

void SettingsComponentSmokeTest::sliderWithoutDetentRemainsContinuous()
{
    SliderFixture fixture;
    QVERIFY(fixture.directory.isValid());

    QString error;
    QVERIFY2(fixture.create(2, &error), qPrintable(error));
    fixture.slider->setProperty("detentValue", 5.0);
    QVERIFY(!fixture.slider->property("detentEnabled").toBool());
    QSignalSpy edited(fixture.slider, SIGNAL(valueEdited(double)));
    QVERIFY(edited.isValid());

    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                      fixture.renderedHandleCenter());
    QTest::mouseMove(&fixture.window, fixture.pointForVisualPosition(0.37));
    QCoreApplication::processEvents();
    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                        fixture.pointForVisualPosition(0.37));

    QVERIFY(!edited.isEmpty());
    QVERIFY(qAbs(fixture.slider->property("value").toReal() - 3.7) < 0.2);
    QVERIFY(qAbs(edited.constLast().at(0).toReal() - 5.0) > 0.2);
}

void SettingsComponentSmokeTest::sliderEntersDefaultDetent()
{
    SliderFixture fixture;
    QVERIFY(fixture.directory.isValid());

    QString error;
    QVERIFY2(fixture.create(2, &error), qPrintable(error));
    fixture.slider->setProperty("detentEnabled", true);
    fixture.slider->setProperty("detentValue", 5.0);
    QSignalSpy edited(fixture.slider, SIGNAL(valueEdited(double)));
    QVERIFY(edited.isValid());

    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                      fixture.renderedHandleCenter());
    QTest::mouseMove(&fixture.window, fixture.pointForVisualPosition(0.475));
    QCoreApplication::processEvents();

    QCOMPARE(fixture.slider->property("displayedValue").toReal(), 5.0);
    QVERIFY(fixture.slider->property("detentLatched").toBool());
    QVERIFY(!edited.isEmpty());
    QCOMPARE(edited.constLast().at(0).toReal(), 5.0);
    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                        fixture.pointForVisualPosition(0.475));
    QVERIFY(!fixture.slider->property("detentLatched").toBool());
}

void SettingsComponentSmokeTest::sliderDetentUsesHysteresis()
{
    SliderFixture fixture;
    QVERIFY(fixture.directory.isValid());

    QString error;
    QVERIFY2(fixture.create(2, &error), qPrintable(error));
    fixture.slider->setProperty("detentEnabled", true);
    fixture.slider->setProperty("detentValue", 5.0);
    fixture.slider->setProperty("detentSnapDistancePx", 8.0);
    fixture.slider->setProperty("detentReleaseDistancePx", 14.0);

    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                      fixture.renderedHandleCenter());
    QTest::mouseMove(&fixture.window, fixture.pointForVisualPosition(0.475));
    QCOMPARE(fixture.slider->property("displayedValue").toReal(), 5.0);
    QVERIFY(fixture.slider->property("detentLatched").toBool());

    QTest::mouseMove(&fixture.window, fixture.pointForVisualPosition(0.54));
    QCOMPARE(fixture.slider->property("displayedValue").toReal(), 5.0);
    QVERIFY(fixture.slider->property("detentLatched").toBool());

    QTest::mouseMove(&fixture.window, fixture.pointForVisualPosition(0.60));
    QVERIFY(fixture.slider->property("displayedValue").toReal() > 5.0);
    QVERIFY(!fixture.slider->property("detentLatched").toBool());
    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                        fixture.pointForVisualPosition(0.60));
}

void SettingsComponentSmokeTest::sliderDetentPulseIsOneShotPerEntry()
{
    SliderFixture fixture;
    QVERIFY(fixture.directory.isValid());

    QString error;
    QVERIFY2(fixture.create(2, &error), qPrintable(error));
    fixture.slider->setProperty("detentEnabled", true);
    fixture.slider->setProperty("detentValue", 5.0);

    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                      fixture.renderedHandleCenter());
    QTest::mouseMove(&fixture.window, fixture.pointForVisualPosition(0.475));
    QCOMPARE(fixture.slider->property("pulseSerial").toInt(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(fixture.slider->property("pulseScale").toReal() > 1.02, 100);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(fixture.slider->property("pulseScale").toReal() - 1.0) < 0.01,
                             300);

    QTest::mouseMove(&fixture.window, fixture.pointForVisualPosition(0.54));
    QTest::qWait(30);
    QVERIFY(fixture.slider->property("pulseScale").toReal() < 1.02);
    QCOMPARE(fixture.slider->property("pulseSerial").toInt(), 1);
    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                        fixture.pointForVisualPosition(0.54));

    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                      fixture.renderedHandleCenter());
    QTest::mouseMove(&fixture.window, fixture.pointForVisualPosition(0.475));
    QCOMPARE(fixture.slider->property("pulseSerial").toInt(), 2);
    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                        fixture.pointForVisualPosition(0.475));
}

void SettingsComponentSmokeTest::sliderControlledBindingSurvivesDetent()
{
    SliderFixture fixture;
    QVERIFY(fixture.directory.isValid());

    QString error;
    QVERIFY2(fixture.createControlled(2, &error), qPrintable(error));
    QVERIFY(fixture.fixtureRoot != nullptr);
    QCOMPARE(fixture.fixtureRoot->property("backendValue").toReal(), 2.0);

    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                      fixture.renderedHandleCenter());
    QTest::mouseMove(&fixture.window, fixture.pointForVisualPosition(0.475));
    QCOMPARE(fixture.fixtureRoot->property("backendValue").toReal(), 5.0);
    QCOMPARE(fixture.slider->property("displayedValue").toReal(), 5.0);
    QVERIFY(fixture.slider->property("detentLatched").toBool());
    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                        fixture.pointForVisualPosition(0.475));
    QVERIFY(!fixture.slider->property("detentLatched").toBool());
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(fixture.slider->property("value").toReal() - 5.0) < 0.001,
                             100);

    fixture.fixtureRoot->setProperty("backendValue", 7.0);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(fixture.slider->property("value").toReal() - 7.0) < 0.001,
                             100);
    QCOMPARE(fixture.slider->property("modelValue").toReal(), 7.0);
    QCOMPARE(fixture.slider->property("displayedValue").toReal(), 7.0);
    QCOMPARE(fixture.slider->property("valueText").toString(), QStringLiteral("7.0"));
    QVERIFY(!fixture.slider->property("detentLatched").toBool());
    const QPoint expectedHandleCenter = fixture.pointForVisualPosition(0.7);
    const QPoint actualHandleCenter = fixture.renderedHandleCenter();
    QVERIFY2(qAbs(actualHandleCenter.x() - expectedHandleCenter.x()) <= 1
                 && qAbs(actualHandleCenter.y() - expectedHandleCenter.y()) <= 1,
             qPrintable(QStringLiteral("expected (%1, %2), got (%3, %4)")
                            .arg(expectedHandleCenter.x())
                            .arg(expectedHandleCenter.y())
                            .arg(actualHandleCenter.x())
                            .arg(actualHandleCenter.y())));
}

void SettingsComponentSmokeTest::sliderControlledPressWithoutMoveDoesNotJump()
{
    SliderFixture fixture;
    QVERIFY(fixture.directory.isValid());

    QString error;
    QVERIFY2(fixture.createControlled(7, &error), qPrintable(error));
    QVERIFY(fixture.fixtureRoot != nullptr);
    QCOMPARE(fixture.fixtureRoot->property("backendValue").toReal(), 7.0);
    QCOMPARE(fixture.slider->property("modelValue").toReal(), 7.0);
    QCOMPARE(fixture.slider->property("value").toReal(), 7.0);
    QCOMPARE(fixture.slider->property("displayedValue").toReal(), 7.0);
    const QPoint handleCenter = fixture.renderedHandleCenter();
    QSignalSpy nativeValueChanges(fixture.slider, SIGNAL(valueChanged()));
    QVERIFY(nativeValueChanges.isValid());

    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier, handleCenter);
    QCoreApplication::processEvents();

    QVERIFY(fixture.slider->property("pressed").toBool());
    QVERIFY2(nativeValueChanges.isEmpty(),
             qPrintable(QStringLiteral("native value changed %1 times on press")
                            .arg(nativeValueChanges.count())));
    QCOMPARE(fixture.fixtureRoot->property("backendValue").toReal(), 7.0);
    QCOMPARE(fixture.slider->property("value").toReal(), 7.0);
    QCOMPARE(fixture.slider->property("displayedValue").toReal(), 7.0);
    QCOMPARE(fixture.renderedHandleCenter(), handleCenter);

    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::NoModifier, handleCenter);
    QCoreApplication::processEvents();

    QCOMPARE(fixture.fixtureRoot->property("backendValue").toReal(), 7.0);
    QCOMPARE(fixture.slider->property("value").toReal(), 7.0);
    QCOMPARE(fixture.slider->property("displayedValue").toReal(), 7.0);
    QCOMPARE(fixture.renderedHandleCenter(), handleCenter);
}

void SettingsComponentSmokeTest::sliderControlledKeyboardEditStartsFromModelValue()
{
    SliderFixture fixture;
    QVERIFY(fixture.directory.isValid());

    QString error;
    QVERIFY2(fixture.createControlled(7, &error), qPrintable(error));
    fixture.slider->forceActiveFocus();
    QVERIFY(fixture.slider->property("activeFocus").toBool());

    QTest::keyClick(&fixture.window, Qt::Key_Right);
    QCoreApplication::processEvents();

    QVERIFY2(qAbs(fixture.fixtureRoot->property("backendValue").toReal() - 7.1) < 0.001,
             qPrintable(QStringLiteral("backend=%1")
                            .arg(fixture.fixtureRoot->property("backendValue").toReal())));
    QVERIFY2(qAbs(fixture.slider->property("value").toReal() - 7.1) < 0.001,
             qPrintable(QStringLiteral("value=%1").arg(fixture.slider->property("value").toReal())));
}

void SettingsComponentSmokeTest::sliderControlledModelChangeDuringDragDefersNativeSync()
{
    SliderFixture fixture;
    QVERIFY(fixture.directory.isValid());

    QString error;
    QVERIFY2(fixture.createControlled(2, &error), qPrintable(error));
    const QPoint dragPoint = fixture.pointForVisualPosition(0.3);

    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                      fixture.renderedHandleCenter());
    QTest::mouseMove(&fixture.window, dragPoint);
    QCoreApplication::processEvents();
    QVERIFY(fixture.slider->property("pressed").toBool());
    QVERIFY(fixture.slider->property("value").toReal() < 4.0);

    fixture.fixtureRoot->setProperty("backendValue", 8.0);
    QCoreApplication::processEvents();
    QVERIFY2(qAbs(fixture.slider->property("value").toReal() - 8.0) > 0.5,
             qPrintable(QStringLiteral("value was forced to %1 during drag")
                            .arg(fixture.slider->property("value").toReal())));

    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::NoModifier, dragPoint);
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.slider->property("pressed").toBool(), 100);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(fixture.slider->property("value").toReal() - 8.0) < 0.001,
                             100);
    QCOMPARE(fixture.slider->property("modelValue").toReal(), 8.0);
}

void SettingsComponentSmokeTest::sliderLatchClearsOnReleaseAndRearmsSnap()
{
    SliderFixture fixture;
    QVERIFY(fixture.directory.isValid());

    QString error;
    QVERIFY2(fixture.create(2, &error), qPrintable(error));
    fixture.slider->setProperty("detentEnabled", true);
    fixture.slider->setProperty("detentValue", 5.0);
    fixture.slider->setProperty("detentSnapDistancePx", 8.0);
    fixture.slider->setProperty("detentReleaseDistancePx", 14.0);
    auto *marker = qobject_cast<QQuickItem *>(
        fixture.slider->findChild<QObject *>(QStringLiteral("sliderDefaultDetentMarker")));
    QVERIFY(marker != nullptr);

    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                      fixture.renderedHandleCenter());
    QTest::mouseMove(&fixture.window, fixture.pointForVisualPosition(0.475));
    QVERIFY(fixture.slider->property("detentLatched").toBool());
    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                        fixture.pointForVisualPosition(0.475));
    QVERIFY(!fixture.slider->property("detentLatched").toBool());
    QTRY_VERIFY_WITH_TIMEOUT(marker->property("opacity").toReal() < 0.8, 200);

    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                      fixture.renderedHandleCenter());
    QTest::mouseMove(&fixture.window, fixture.pointForVisualPosition(0.55));
    QVERIFY(!fixture.slider->property("detentLatched").toBool());
    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::NoModifier,
                        fixture.pointForVisualPosition(0.55));
}

void SettingsComponentSmokeTest::sliderKeyboardCrossesDefaultDetent()
{
    SliderFixture fixture;
    QVERIFY(fixture.directory.isValid());

    QString error;
    QVERIFY2(fixture.create(4.8, &error), qPrintable(error));
    fixture.slider->setProperty("detentEnabled", true);
    fixture.slider->setProperty("detentValue", 5.0);
    fixture.slider->forceActiveFocus();
    QVERIFY(fixture.slider->property("activeFocus").toBool());

    QTest::keyClick(&fixture.window, Qt::Key_Right);
    QVERIFY(fixture.slider->property("value").toReal() > 4.8);
    for (int i = 0; i < 5; ++i)
        QTest::keyClick(&fixture.window, Qt::Key_Right);
    const qreal valueAfterKeys = fixture.slider->property("value").toReal();
    QVERIFY2(valueAfterKeys > 5.0, qPrintable(QStringLiteral("value=%1").arg(valueAfterKeys)));
}

void SettingsComponentSmokeTest::sliderMarkerUsesMirroredTrackGeometry()
{
    SliderFixture fixture;
    QVERIFY(fixture.directory.isValid());

    QString error;
    QVERIFY2(fixture.create(0, &error), qPrintable(error));
    fixture.slider->setProperty("detentEnabled", true);
    fixture.slider->setProperty("detentValue", 2.5);
    auto *marker = qobject_cast<QQuickItem *>(
        fixture.slider->findChild<QObject *>(QStringLiteral("sliderDefaultDetentMarker")));
    QVERIFY(marker != nullptr);

    const qreal trackStart = fixture.slider->property("trackStart").toReal();
    const qreal trackWidth = fixture.slider->property("trackWidth").toReal();
    const qreal markerCenter = marker->mapToItem(fixture.slider,
                                                  QPointF(marker->width() / 2.0,
                                                          marker->height() / 2.0)).x();
    QVERIFY(qAbs(markerCenter - (trackStart + trackWidth * 0.25)) < 1.0);

    QVERIFY(fixture.slider->setProperty("mirrorForTest", true));
    QCoreApplication::processEvents();
    const qreal mirroredCenter = marker->mapToItem(
        fixture.slider, QPointF(marker->width() / 2.0, marker->height() / 2.0)).x();
    QVERIFY(qAbs(mirroredCenter - (trackStart + trackWidth * 0.75)) < 1.0);
}

QTEST_MAIN(SettingsComponentSmokeTest)
#include "SettingsComponentSmokeTest.moc"
