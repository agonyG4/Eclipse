#include "core/navigation/SettingsNavigationModel.hpp"

#include <QSet>
#include <QtTest>

class SettingsNavigationModelTest final : public QObject {
    Q_OBJECT

private slots:
    void exposesExactCatalogueOrder();
    void selectableRowsHaveUniqueNonEmptyIds();
    void preservesSpacerBehavior();
    void exposesNativePageRoutes();
};

void SettingsNavigationModelTest::exposesExactCatalogueOrder()
{
    SettingsNavigationModel model;
    const QStringList expected{
        QStringLiteral("system"), QStringLiteral("software-update"), QStringLiteral("internet"),
        QStringLiteral("bluetooth"), QStringLiteral("audio"), QStringLiteral("components"),
        QStringLiteral("services"), QStringLiteral("compositor"), QString(),
        QStringLiteral("performance"), QStringLiteral("appearance"), QStringLiteral("wallpaper"),
        QStringLiteral("more-settings"),
    };

    QCOMPARE(model.rowCount(), expected.size());
    for (int row = 0; row < expected.size(); ++row)
        QCOMPARE(model.get(row).value(QStringLiteral("entryId")).toString(), expected.at(row));
}

void SettingsNavigationModelTest::selectableRowsHaveUniqueNonEmptyIds()
{
    SettingsNavigationModel model;
    QSet<QString> ids;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QVariantMap entry = model.get(row);
        if (entry.value(QStringLiteral("kind")).toString() == QStringLiteral("spacer"))
            continue;
        const QString id = entry.value(QStringLiteral("entryId")).toString();
        QVERIFY(!id.isEmpty());
        QVERIFY(!ids.contains(id));
        ids.insert(id);
        QVERIFY(model.containsSelectableId(id));
    }
}

void SettingsNavigationModelTest::preservesSpacerBehavior()
{
    SettingsNavigationModel model;
    const QModelIndex spacer = model.index(8, 0);
    QCOMPARE(model.data(spacer, SettingsNavigationModel::KindRole).toString(), QStringLiteral("spacer"));
    QVERIFY(!model.data(spacer, SettingsNavigationModel::EnabledRole).toBool());
    QVERIFY(!model.data(spacer, SettingsNavigationModel::SelectedRole).toBool());
    QVERIFY(!model.setSelectedId(QString()));
    QCOMPARE(model.selectedId(), QStringLiteral("system"));
}

void SettingsNavigationModelTest::exposesNativePageRoutes()
{
    SettingsNavigationModel model;
    const QUrl compositorRoute(
        QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Compositor.qml"));
    const QUrl wallpaperRoute(
        QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Wallpaper.qml"));

    for (int row = 0; row < model.rowCount(); ++row) {
        const QVariantMap entry = model.get(row);
        const QString id = entry.value(QStringLiteral("entryId")).toString();
        const QUrl route = entry.value(QStringLiteral("pageSource")).toUrl();
        if (id == QStringLiteral("compositor"))
            QCOMPARE(route, compositorRoute);
        else if (id == QStringLiteral("wallpaper"))
            QCOMPARE(route, wallpaperRoute);
        else
            QVERIFY(route.isEmpty());
    }
}

QTEST_GUILESS_MAIN(SettingsNavigationModelTest)
#include "SettingsNavigationModelTest.moc"
