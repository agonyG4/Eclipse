#include "core/SettingsController.hpp"
#include "core/navigation/SettingsNavigationModel.hpp"

#include <QSignalSpy>
#include <QtTest>

class SettingsControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithFirstRoutablePage();
    void rejectsUnknownOrUnavailableSelection();
    void exposesSelectedRole();
    void selectsCompositor();
    void exposesSelectedPageSource();
    void unavailableSelectionPreservesRouteAndSignal();
    void usesInjectedProfileForIsSudo();
};

void SettingsControllerTest::startsWithFirstRoutablePage()
{
    SettingsController controller;

    QCOMPARE(controller.selectedSectionId(), QStringLiteral("compositor"));
    QCOMPARE(controller.selectedSectionTitle(), QStringLiteral("Compositor"));
    QCOMPARE(controller.selectedPageSource(),
             QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Compositor.qml")));
    QCOMPARE(controller.navigationModel()->rowCount(), 14);
}

void SettingsControllerTest::rejectsUnknownOrUnavailableSelection()
{
    SettingsController controller;
    QSignalSpy selectionSpy(&controller, &SettingsController::selectionChanged);

    QVERIFY(!controller.selectSection(QStringLiteral("missing")));
    QVERIFY(!controller.selectSection(QStringLiteral("appearance")));
    QVERIFY(!controller.selectSection(QStringLiteral("system")));
    QCOMPARE(controller.selectedSectionId(), QStringLiteral("compositor"));
    QCOMPARE(selectionSpy.count(), 0);
}

void SettingsControllerTest::exposesSelectedRole()
{
    SettingsController controller;
    SettingsNavigationModel *model = controller.navigationModel();

    QVERIFY(controller.selectSection(QStringLiteral("compositor")));

    int selectedRows = 0;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (model->data(index, SettingsNavigationModel::SelectedRole).toBool()) {
            ++selectedRows;
            QCOMPARE(model->data(index, SettingsNavigationModel::IdRole).toString(),
                     QStringLiteral("compositor"));
        }
    }
    QCOMPARE(selectedRows, 1);
}

void SettingsControllerTest::selectsCompositor()
{
    SettingsController controller;
    SettingsNavigationModel *model = controller.navigationModel();

    QVERIFY(controller.selectSection(QStringLiteral("compositor")));
    QCOMPARE(controller.selectedSectionId(), QStringLiteral("compositor"));
    QCOMPARE(controller.selectedSectionTitle(), QStringLiteral("Compositor"));

    const QVariantMap entry = model->get(7);
    QCOMPARE(entry.value(QStringLiteral("labelKey")).toString(), QStringLiteral("settings.nav.compositor"));
    QCOMPARE(entry.value(QStringLiteral("subtitle")).toString(), QStringLiteral("Astrea compositor preferences"));
    QCOMPARE(entry.value(QStringLiteral("kind")).toString(), QStringLiteral("page"));
}

void SettingsControllerTest::exposesSelectedPageSource()
{
    SettingsController controller;

    QCOMPARE(controller.selectedPageSource(),
             QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Compositor.qml")));
    QVERIFY(controller.selectSection(QStringLiteral("compositor")));
    QCOMPARE(controller.selectedPageSource(),
             QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Compositor.qml")));
}

void SettingsControllerTest::unavailableSelectionPreservesRouteAndSignal()
{
    SettingsController controller;
    const QUrl route = controller.selectedPageSource();

    QSignalSpy selectionSpy(&controller, &SettingsController::selectionChanged);
    QVERIFY(!controller.selectSection(QStringLiteral("software-update")));
    QVERIFY(!controller.selectSection(QStringLiteral("more-settings")));
    QCOMPARE(controller.selectedPageSource(), route);
    QCOMPARE(controller.selectedSectionId(), QStringLiteral("compositor"));
    QCOMPARE(selectionSpy.count(), 0);
}

void SettingsControllerTest::usesInjectedProfileForIsSudo()
{
    SettingsUserProfile profile;
    profile.userName = QStringLiteral("test-user");
    profile.administrator = true;
    SettingsController controller{profile};

    QVERIFY(controller.isSudo());
    QVERIFY(controller.property("isSudo").toBool());
}

QTEST_MAIN(SettingsControllerTest)
#include "SettingsControllerTest.moc"
