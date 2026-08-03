#include "core/SettingsController.hpp"
#include "core/navigation/SettingsNavigationModel.hpp"

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
    void groupRowsAreSelectable();
    void exposesExactCatalogueOrdering();
    void selectsCompositor();
    void exposesSelectedPageSource();
    void rejectsSpacerSelection();
    void rejectsInvalidSelectionWithoutChangingRoute();
    void filtersCompositor();
    void keepsOneSelectedRowForEverySelection();
    void usesInjectedProfileForIsSudo();
};

void SettingsControllerTest::startsWithSystemSelected()
{
    SettingsController controller;

    QCOMPARE(controller.selectedSectionId(), QStringLiteral("system"));
    QCOMPARE(controller.selectedSectionTitle(), QStringLiteral("System"));
    QCOMPARE(controller.navigationModel()->rowCount(), 12);
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
    QVERIFY(controller.navigationModel()->rowCount() < 12);
    controller.clearFilter();
    QCOMPARE(controller.navigationModel()->rowCount(), 12);
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

void SettingsControllerTest::groupRowsAreSelectable()
{
    SettingsController controller;
    SettingsNavigationModel *model = controller.navigationModel();

    for (const QString &id : {QStringLiteral("performance"), QStringLiteral("appearance"), QStringLiteral("more-settings")}) {
        QVERIFY(controller.selectSection(id));
        QCOMPARE(controller.selectedSectionId(), id);

        bool found = false;
        for (int row = 0; row < model->rowCount(); ++row) {
            const QModelIndex index = model->index(row, 0);
            if (model->data(index, SettingsNavigationModel::IdRole).toString() == id) {
                QCOMPARE(model->data(index, SettingsNavigationModel::KindRole).toString(), QStringLiteral("group"));
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }
}

void SettingsControllerTest::exposesExactCatalogueOrdering()
{
    SettingsController controller;
    SettingsNavigationModel *model = controller.navigationModel();
    const QStringList expectedIds{
        QStringLiteral("system"),
        QStringLiteral("software-update"),
        QStringLiteral("internet"),
        QStringLiteral("bluetooth"),
        QStringLiteral("audio"),
        QStringLiteral("components"),
        QStringLiteral("services"),
        QStringLiteral("compositor"),
        QString(),
        QStringLiteral("performance"),
        QStringLiteral("appearance"),
        QStringLiteral("more-settings"),
    };

    QCOMPARE(model->rowCount(), expectedIds.size());
    for (int row = 0; row < expectedIds.size(); ++row)
        QCOMPARE(model->get(row).value(QStringLiteral("entryId")).toString(), expectedIds.at(row));
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

    QVERIFY(controller.selectedPageSource().isEmpty());
    QVERIFY(controller.selectSection(QStringLiteral("compositor")));
    QCOMPARE(controller.selectedPageSource(),
             QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Compositor.qml")));
}

void SettingsControllerTest::rejectsSpacerSelection()
{
    SettingsController controller;

    QVERIFY(!controller.selectSection(QString()));
    QCOMPARE(controller.selectedSectionId(), QStringLiteral("system"));
}

void SettingsControllerTest::rejectsInvalidSelectionWithoutChangingRoute()
{
    SettingsController controller;
    QVERIFY(controller.selectSection(QStringLiteral("compositor")));
    const QUrl route = controller.selectedPageSource();

    QSignalSpy selectionSpy(&controller, &SettingsController::selectionChanged);
    QVERIFY(!controller.selectSection(QStringLiteral("missing")));
    QCOMPARE(controller.selectedPageSource(), route);
    QCOMPARE(selectionSpy.count(), 0);
}

void SettingsControllerTest::filtersCompositor()
{
    SettingsController controller;
    SettingsNavigationModel *model = controller.navigationModel();

    controller.setFilterText(QStringLiteral("COMPOSITOR"));
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->get(0).value(QStringLiteral("entryId")).toString(), QStringLiteral("compositor"));
}

void SettingsControllerTest::keepsOneSelectedRowForEverySelection()
{
    SettingsController controller;
    SettingsNavigationModel *model = controller.navigationModel();
    const QStringList selectableIds{
        QStringLiteral("system"),
        QStringLiteral("software-update"),
        QStringLiteral("internet"),
        QStringLiteral("bluetooth"),
        QStringLiteral("audio"),
        QStringLiteral("components"),
        QStringLiteral("services"),
        QStringLiteral("compositor"),
        QStringLiteral("performance"),
        QStringLiteral("appearance"),
        QStringLiteral("more-settings"),
    };

    for (const QString &id : selectableIds) {
        QVERIFY(controller.selectSection(id));
        int selectedRows = 0;
        for (int row = 0; row < model->rowCount(); ++row) {
            if (model->data(model->index(row, 0), SettingsNavigationModel::SelectedRole).toBool())
                ++selectedRows;
        }
        QCOMPARE(selectedRows, 1);
    }
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
