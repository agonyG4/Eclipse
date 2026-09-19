#include "core/SettingsController.hpp"

#include <QMetaMethod>
#include <QtTest>

class SettingsThemesControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void exposesThemesControllerThroughSettingsController();
};

void SettingsThemesControllerTest::exposesThemesControllerThroughSettingsController()
{
    SettingsController controller;
    QVERIFY(controller.themes() != nullptr);
    QVERIFY(controller.property("themes").value<QObject *>() == controller.themes());
    QVERIFY(controller.themes()->metaObject()->indexOfMethod("refresh()") >= 0);
    QVERIFY(controller.themes()->metaObject()->indexOfMethod("setIconTheme(QString)") >= 0);
    QVERIFY(controller.themes()->metaObject()->indexOfMethod("useSystemDefault()") >= 0);
}

QTEST_GUILESS_MAIN(SettingsThemesControllerTest)
#include "SettingsThemesControllerTest.moc"
