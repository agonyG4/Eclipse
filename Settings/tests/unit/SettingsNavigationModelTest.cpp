#include "core/navigation/SettingsNavigationModel.hpp"

#include <QSet>
#include <QtTest>

class SettingsNavigationModelTest final : public QObject {
    Q_OBJECT

private slots:
    void exposesExactCatalogueOrder();
    void exposesCanonicalKindsAndRelationships();
    void allRowsHaveUniqueStableIds();
    void preservesSpacerBehavior();
    void exposesNativePageRoutes();
    void startsWithFirstRoutablePage();
    void rejectsUnavailableEntries();
    void togglesSectionsByStableId();
    void keepsSelectedChildVisibleWhenCollapsed();
};

void SettingsNavigationModelTest::exposesExactCatalogueOrder()
{
    SettingsNavigationModel model;
    const QStringList expected{
        QStringLiteral("system"), QStringLiteral("software-update"), QStringLiteral("internet"),
        QStringLiteral("bluetooth"), QStringLiteral("audio"), QStringLiteral("components"),
        QStringLiteral("services"), QStringLiteral("compositor"), QString(),
        QStringLiteral("performance"), QStringLiteral("appearance"), QStringLiteral("wallpaper"),
        QStringLiteral("dock"),
        QStringLiteral("more-settings"),
    };

    QCOMPARE(model.rowCount(), expected.size());
    for (int row = 0; row < expected.size(); ++row)
        QCOMPARE(model.get(row).value(QStringLiteral("entryId")).toString(), expected.at(row));
}

void SettingsNavigationModelTest::exposesCanonicalKindsAndRelationships()
{
    SettingsNavigationModel model;
    const QStringList expectedKinds{
        QStringLiteral("page"), QStringLiteral("page"), QStringLiteral("page"),
        QStringLiteral("page"), QStringLiteral("page"), QStringLiteral("page"),
        QStringLiteral("page"), QStringLiteral("page"), QStringLiteral("spacer"),
        QStringLiteral("section"), QStringLiteral("section"), QStringLiteral("child"),
        QStringLiteral("child"), QStringLiteral("section"),
    };
    const QStringList expectedSectionKeys{
        QString(), QString(), QString(), QString(), QString(), QString(), QString(),
        QString(), QString(), QStringLiteral("performance"), QStringLiteral("appearance"),
        QString(), QString(), QStringLiteral("more-settings"),
    };
    const QStringList expectedParents{
        QString(), QString(), QString(), QString(), QString(), QString(), QString(),
        QString(), QString(), QString(), QString(), QStringLiteral("appearance"),
        QStringLiteral("appearance"), QString(),
    };

    for (int row = 0; row < model.rowCount(); ++row) {
        const QVariantMap entry = model.get(row);
        QCOMPARE(entry.value(QStringLiteral("kind")).toString(), expectedKinds.at(row));
        QCOMPARE(entry.value(QStringLiteral("sectionKey")).toString(), expectedSectionKeys.at(row));
        QCOMPARE(entry.value(QStringLiteral("parentSection")).toString(), expectedParents.at(row));
    }
}

void SettingsNavigationModelTest::allRowsHaveUniqueStableIds()
{
    SettingsNavigationModel model;
    QSet<QString> ids;
    int routableRows = 0;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QVariantMap entry = model.get(row);
        const QString id = entry.value(QStringLiteral("entryId")).toString();
        if (entry.value(QStringLiteral("kind")).toString() == QStringLiteral("spacer")) {
            QVERIFY(id.isEmpty());
            QVERIFY(!entry.value(QStringLiteral("entryEnabled")).toBool());
            continue;
        }
        QVERIFY(!id.isEmpty());
        QVERIFY(!ids.contains(id));
        ids.insert(id);
        const bool routable = entry.value(QStringLiteral("entryEnabled")).toBool();
        QCOMPARE(model.containsSelectableId(id), routable);
        if (routable)
            ++routableRows;
    }
    QCOMPARE(routableRows, 3);
}

void SettingsNavigationModelTest::preservesSpacerBehavior()
{
    SettingsNavigationModel model;
    const QModelIndex spacer = model.index(8, 0);
    QCOMPARE(model.data(spacer, SettingsNavigationModel::KindRole).toString(), QStringLiteral("spacer"));
    QVERIFY(!model.data(spacer, SettingsNavigationModel::EnabledRole).toBool());
    QVERIFY(!model.data(spacer, SettingsNavigationModel::SelectedRole).toBool());
    QVERIFY(!model.setSelectedId(QString()));
    QCOMPARE(model.selectedId(), QStringLiteral("compositor"));
}

void SettingsNavigationModelTest::exposesNativePageRoutes()
{
    SettingsNavigationModel model;
    const QUrl compositorRoute(
        QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Compositor.qml"));
    const QUrl wallpaperRoute(
        QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Wallpaper.qml"));
    const QUrl dockRoute(
        QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Dock.qml"));

    QSet<QString> routableIds;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QVariantMap entry = model.get(row);
        const QString id = entry.value(QStringLiteral("entryId")).toString();
        const QUrl route = entry.value(QStringLiteral("pageSource")).toUrl();
        if (id == QStringLiteral("compositor")) {
            QCOMPARE(route, compositorRoute);
            routableIds.insert(id);
        } else if (id == QStringLiteral("wallpaper")) {
            QCOMPARE(route, wallpaperRoute);
            routableIds.insert(id);
        } else if (id == QStringLiteral("dock")) {
            QCOMPARE(route, dockRoute);
            routableIds.insert(id);
        } else {
            QVERIFY(route.isEmpty());
            QVERIFY(!entry.value(QStringLiteral("entryEnabled")).toBool());
        }
    }
    const QSet<QString> expectedRoutableIds{
        QStringLiteral("compositor"), QStringLiteral("wallpaper"), QStringLiteral("dock")};
    QCOMPARE(routableIds, expectedRoutableIds);
}

void SettingsNavigationModelTest::startsWithFirstRoutablePage()
{
    SettingsNavigationModel model;

    QCOMPARE(model.selectedId(), QStringLiteral("compositor"));
    QCOMPARE(model.pageSourceForId(model.selectedId()),
             QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Compositor.qml")));
}

void SettingsNavigationModelTest::rejectsUnavailableEntries()
{
    SettingsNavigationModel model;
    QSignalSpy selectedSpy(&model, &SettingsNavigationModel::selectedIdChanged);

    for (const QString &id : {QStringLiteral("system"), QStringLiteral("performance"),
                              QStringLiteral("appearance"), QStringLiteral("more-settings")}) {
        QVERIFY(!model.containsSelectableId(id));
        QVERIFY(!model.setSelectedId(id));
        QCOMPARE(model.selectedId(), QStringLiteral("compositor"));
    }
    QCOMPARE(selectedSpy.count(), 0);
}

void SettingsNavigationModelTest::togglesSectionsByStableId()
{
    SettingsNavigationModel model;

    QCOMPARE(model.rowCount(), 14);
    QVERIFY(model.toggleSection(QStringLiteral("appearance")));
    QCOMPARE(model.rowCount(), 12);
    QVERIFY(!model.containsSelectableId(QStringLiteral("appearance")));
    QVERIFY(!model.toggleSection(QStringLiteral("wallpaper")));
    QVERIFY(model.toggleSection(QStringLiteral("appearance")));
    QCOMPARE(model.rowCount(), 14);
    QVERIFY(!model.toggleSection(QStringLiteral("missing")));
}

void SettingsNavigationModelTest::keepsSelectedChildVisibleWhenCollapsed()
{
    SettingsNavigationModel model;
    QVERIFY(model.setSelectedId(QStringLiteral("wallpaper")));
    QVERIFY(model.toggleSection(QStringLiteral("appearance")));

    QCOMPARE(model.rowCount(), 13);
    bool wallpaperVisible = false;
    bool dockVisible = false;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QString id = model.get(row).value(QStringLiteral("entryId")).toString();
        wallpaperVisible |= id == QStringLiteral("wallpaper");
        dockVisible |= id == QStringLiteral("dock");
    }
    QVERIFY(wallpaperVisible);
    QVERIFY(!dockVisible);
    QVERIFY(model.data(model.index(11, 0), SettingsNavigationModel::SelectedRole).toBool());
}

QTEST_GUILESS_MAIN(SettingsNavigationModelTest)
#include "SettingsNavigationModelTest.moc"
