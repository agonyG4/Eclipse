#include "core/SettingsController.hpp"
#include "core/SettingsNavigationModel.hpp"

#include <QSignalSpy>
#include <QtTest>

class SettingsControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithSystemSelected();
    void selectsKnownSection();
    void rejectsUnknownSection();
    void filtersCaseInsensitively();
    void clearingFilterRestoresCatalogue();
    void exposesSelectedRole();
};

void SettingsControllerTest::startsWithSystemSelected()
{
    SettingsController controller;

    QCOMPARE(controller.selectedSectionId(), QStringLiteral("system"));
    QCOMPARE(controller.selectedSectionTitle(), QStringLiteral("System"));
    QCOMPARE(controller.navigationModel()->rowCount(), 9);
    QVERIFY(!controller.pagesAvailable());
}

void SettingsControllerTest::selectsKnownSection()
{
    SettingsController controller;
    QSignalSpy selectionSpy(&controller, &SettingsController::selectionChanged);

    QVERIFY(controller.selectSection(QStringLiteral("appearance")));
    QCOMPARE(controller.selectedSectionId(), QStringLiteral("appearance"));
    QCOMPARE(controller.selectedSectionTitle(), QStringLiteral("Appearance"));
    QCOMPARE(selectionSpy.count(), 1);
}

void SettingsControllerTest::rejectsUnknownSection()
{
    SettingsController controller;
    QSignalSpy selectionSpy(&controller, &SettingsController::selectionChanged);

    QVERIFY(!controller.selectSection(QStringLiteral("missing")));
    QCOMPARE(controller.selectedSectionId(), QStringLiteral("system"));
    QCOMPARE(selectionSpy.count(), 0);
}

void SettingsControllerTest::filtersCaseInsensitively()
{
    SettingsController controller;
    SettingsNavigationModel *model = controller.navigationModel();

    controller.setFilterText(QStringLiteral("BLUE"));
    QCOMPARE(model->rowCount(), 1);
    const QModelIndex onlyRow = model->index(0, 0);
    QCOMPARE(model->data(onlyRow, SettingsNavigationModel::IdRole).toString(),
             QStringLiteral("bluetooth"));
}

void SettingsControllerTest::clearingFilterRestoresCatalogue()
{
    SettingsController controller;

    controller.setFilterText(QStringLiteral("network"));
    QVERIFY(controller.navigationModel()->rowCount() < 9);
    controller.clearFilter();
    QCOMPARE(controller.navigationModel()->rowCount(), 9);
    QCOMPARE(controller.filterText(), QString());
}

void SettingsControllerTest::exposesSelectedRole()
{
    SettingsController controller;
    SettingsNavigationModel *model = controller.navigationModel();

    QVERIFY(controller.selectSection(QStringLiteral("audio")));

    int selectedRows = 0;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (model->data(index, SettingsNavigationModel::SelectedRole).toBool()) {
            ++selectedRows;
            QCOMPARE(model->data(index, SettingsNavigationModel::IdRole).toString(),
                     QStringLiteral("audio"));
        }
    }
    QCOMPARE(selectedRows, 1);
}

QTEST_MAIN(SettingsControllerTest)
#include "SettingsControllerTest.moc"
