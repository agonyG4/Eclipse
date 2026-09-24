#include "core/SettingsController.hpp"

#include <QMetaMethod>
#include <QtTest>

class SettingsVisualEffectsControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void controllerIsRustBackedAndTyped();
};

void SettingsVisualEffectsControllerTest::controllerIsRustBackedAndTyped()
{
    SettingsController settings;
    auto *controller = settings.visualEffects();
    QVERIFY(controller != nullptr);
    QCOMPARE(QString::fromLatin1(controller->metaObject()->className()),
             QStringLiteral("SettingsVisualEffectsController"));
    QCOMPARE(controller, settings.property("visualEffects").value<SettingsVisualEffectsController *>());

    const auto *meta = controller->metaObject();
    for (const auto *name : {"available", "busy", "materialPosition", "defaultMaterialPosition",
                              "effectiveBlur", "effectiveSaturation", "effectiveNoise",
                              "blurOverrideSupported", "saturationOverrideSupported",
                              "noiseOverrideSupported",
                              "blurOverridden", "saturationOverridden", "noiseOverridden",
                              "hasOverrides", "generation", "source", "lastError"}) {
        QVERIFY2(meta->indexOfProperty(name) >= 0, name);
    }
    for (const auto *name : {"blurOverrideSupported", "saturationOverrideSupported",
                              "noiseOverrideSupported"}) {
        const auto propertyIndex = meta->indexOfProperty(name);
        QVERIFY2(propertyIndex >= 0, name);
        const auto property = meta->property(propertyIndex);
        QVERIFY2(property.isValid(), name);
        QCOMPARE(property.metaType().id(), QMetaType::Bool);
        QVERIFY2(!property.isWritable(), name);
    }
    for (const auto *signature : {"refresh()", "setMaterialPosition(double)",
                                  "setBlurOverride(double)", "clearBlurOverride()",
                                  "setSaturationOverride(double)", "clearSaturationOverride()",
                                  "setNoiseOverride(double)", "clearNoiseOverride()",
                                  "resetOverrides()", "restoreDefaults()", "flush()"}) {
        QVERIFY2(meta->indexOfMethod(QMetaObject::normalizedSignature(signature)) >= 0,
                 signature);
    }
    QVERIFY(meta->indexOfMethod("setOverride(QString,QVariant)") < 0);
}

QTEST_MAIN(SettingsVisualEffectsControllerTest)
#include "SettingsVisualEffectsControllerTest.moc"
