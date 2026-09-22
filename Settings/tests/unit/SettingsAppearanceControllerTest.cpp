#include "core/SettingsController.hpp"

#include <QtTest>

class SettingsAppearanceControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void exposesAppearanceControllerThroughSettingsController();
};

void SettingsAppearanceControllerTest::exposesAppearanceControllerThroughSettingsController()
{
    SettingsController controller;
    QObject *appearance = controller.appearance();
    QVERIFY(appearance != nullptr);
    QVERIFY(controller.property("appearance").value<QObject *>() == appearance);

    const QMetaObject *metaObject = appearance->metaObject();
    for (const char *name : {"busy", "lastError"})
        QVERIFY(metaObject->indexOfProperty(name) >= 0);
    for (const char *method : {"setThemePreference(QString)",
                               "setAccentHex(QString)",
                               "setIconAppearance(QString)"}) {
        QVERIFY(metaObject->indexOfMethod(method) >= 0);
    }
    for (const char *signal : {"busyChanged()", "errorChanged()", "configurationChanged()"})
        QVERIFY(metaObject->indexOfSignal(signal) >= 0);
}

QTEST_GUILESS_MAIN(SettingsAppearanceControllerTest)
#include "SettingsAppearanceControllerTest.moc"
