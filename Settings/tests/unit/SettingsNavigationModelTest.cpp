#include "core/navigation/SettingsNavigationModel.hpp"

#include <QSet>
#include <QtTest>

class SettingsNavigationModelTest final : public QObject {
    Q_OBJECT

private slots:
    void exposesExactSidebarOrder();
    void exposesCanonicalKindsAndVisibility();
    void allCatalogueEntriesHaveUniqueStableIds();
    void preservesSpacerBehavior();
    void exposesNativePageRoutes();
    void exposesHubChildrenAndAncestorLookups();
    void startsWithFirstNavigableSidebarDestination();
    void appliesPageAndHubNavigabilityRules();
};

void SettingsNavigationModelTest::exposesExactSidebarOrder()
{
    SettingsNavigationModel model;
    const QStringList expected{
        QStringLiteral("system"), QStringLiteral("software-update"), QStringLiteral("internet"),
        QStringLiteral("bluetooth"), QStringLiteral("audio"), QStringLiteral("components"),
        QStringLiteral("services"), QStringLiteral("compositor"), QString(),
        QStringLiteral("performance"), QStringLiteral("customization"), QStringLiteral("more-settings"),
    };

    QCOMPARE(model.rowCount(), 12);
    QCOMPARE(model.rowCount(), expected.size());
    for (int row = 0; row < expected.size(); ++row)
        QCOMPARE(model.get(row).value(QStringLiteral("entryId")).toString(), expected.at(row));
}

void SettingsNavigationModelTest::exposesCanonicalKindsAndVisibility()
{
    SettingsNavigationModel model;
    const QStringList expectedKinds{
        QStringLiteral("page"), QStringLiteral("page"), QStringLiteral("page"),
        QStringLiteral("page"), QStringLiteral("page"), QStringLiteral("page"),
        QStringLiteral("page"), QStringLiteral("page"), QStringLiteral("spacer"),
        QStringLiteral("hub"), QStringLiteral("hub"), QStringLiteral("hub"),
    };

    for (int row = 0; row < model.rowCount(); ++row) {
        const QVariantMap entry = model.get(row);
        QCOMPARE(entry.value(QStringLiteral("kind")).toString(), expectedKinds.at(row));
        QVERIFY(entry.value(QStringLiteral("sidebarVisible")).toBool()
                || entry.value(QStringLiteral("kind")).toString() == QStringLiteral("spacer"));
        QVERIFY(entry.value(QStringLiteral("kind")).toString() != QStringLiteral("section"));
        QVERIFY(entry.value(QStringLiteral("kind")).toString() != QStringLiteral("child"));
    }

    const QVariantMap customization = model.descriptorForId(QStringLiteral("customization"));
    QCOMPARE(customization.value(QStringLiteral("kind")).toString(), QStringLiteral("hub"));
    QVERIFY(customization.value(QStringLiteral("sidebarVisible")).toBool());
    QCOMPARE(customization.value(QStringLiteral("subtitleKey")).toString(),
             QStringLiteral("settings.nav.customization.subtitle"));

    for (const QString &id : {QStringLiteral("visual-effects"), QStringLiteral("icons"),
                              QStringLiteral("wallpaper"), QStringLiteral("dock")}) {
        const QVariantMap entry = model.descriptorForId(id);
        QCOMPARE(entry.value(QStringLiteral("kind")).toString(), QStringLiteral("page"));
        QVERIFY(!entry.value(QStringLiteral("sidebarVisible")).toBool());
        QCOMPARE(entry.value(QStringLiteral("parentId")).toString(), QStringLiteral("customization"));
    }
}

void SettingsNavigationModelTest::allCatalogueEntriesHaveUniqueStableIds()
{
    SettingsNavigationCatalog catalog;
    QSet<QString> ids;
    for (const auto &entry : catalog.entries()) {
        if (entry.kind == SettingsNavigationEntry::Kind::Spacer) {
            QVERIFY(entry.id.isEmpty());
            continue;
        }
        QVERIFY(!entry.id.isEmpty());
        QVERIFY(!ids.contains(entry.id));
        ids.insert(entry.id);
    }
    QCOMPARE(ids.size(), catalog.entries().size() - 1);
}

void SettingsNavigationModelTest::preservesSpacerBehavior()
{
    SettingsNavigationModel model;
    const QModelIndex spacer = model.index(8, 0);
    QCOMPARE(model.data(spacer, SettingsNavigationModel::KindRole).toString(), QStringLiteral("spacer"));
    QVERIFY(!model.data(spacer, SettingsNavigationModel::EnabledRole).toBool());
    QCOMPARE(model.data(spacer, SettingsNavigationModel::ParentIdRole).toString(), QString());
}

void SettingsNavigationModelTest::exposesNativePageRoutes()
{
    SettingsNavigationModel model;
    const QUrl compositorRoute(
        QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Compositor.qml"));
    const QUrl bluetoothRoute(
        QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Bluetooth.qml"));
    const QUrl customizationRoute(
        QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/navigation/Hub.qml"));
    const QUrl wallpaperRoute(
        QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Wallpaper.qml"));
    const QUrl dockRoute(
        QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Dock.qml"));
    const QUrl appearanceRoute(
        QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Appearance.qml"));
    const QUrl animationsRoute(
        QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Animations.qml"));
    const QUrl visualEffectsRoute(
        QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/VisualEffects.qml"));
    const QUrl iconsRoute(
        QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Icons.qml"));

    QCOMPARE(model.pageSourceForId(QStringLiteral("compositor")), compositorRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("bluetooth")), bluetoothRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("customization")), customizationRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("wallpaper")), wallpaperRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("dock")), dockRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("appearance")), appearanceRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("animations")), animationsRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("visual-effects")), visualEffectsRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("icons")), iconsRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("system")), QUrl());
    QCOMPARE(model.pageSourceForId(QStringLiteral("missing")), QUrl());
}

void SettingsNavigationModelTest::exposesHubChildrenAndAncestorLookups()
{
    SettingsNavigationModel model;

    const auto *customization = model.entryForId(QStringLiteral("customization"));
    QVERIFY(customization != nullptr);
    QCOMPARE(customization->kind, SettingsNavigationEntry::Kind::Hub);

    const QVector<SettingsNavigationEntry> children = model.childrenForId(QStringLiteral("customization"));
    QCOMPARE(children.size(), 6);
    QCOMPARE(children.at(0).id, QStringLiteral("appearance"));
    QCOMPARE(children.at(1).id, QStringLiteral("visual-effects"));
    QCOMPARE(children.at(2).id, QStringLiteral("icons"));
    QCOMPARE(children.at(3).id, QStringLiteral("wallpaper"));
    QCOMPARE(children.at(4).id, QStringLiteral("dock"));
    QCOMPARE(children.at(5).id, QStringLiteral("animations"));

    const QVariantList childDescriptors = model.childDescriptorsForId(QStringLiteral("customization"));
    QCOMPARE(childDescriptors.size(), 6);
    QCOMPARE(childDescriptors.at(0).toMap().value(QStringLiteral("entryId")).toString(),
             QStringLiteral("appearance"));
    QCOMPARE(childDescriptors.at(1).toMap().value(QStringLiteral("entryId")).toString(),
             QStringLiteral("visual-effects"));
    QCOMPARE(childDescriptors.at(2).toMap().value(QStringLiteral("entryId")).toString(),
             QStringLiteral("icons"));
    QCOMPARE(childDescriptors.at(3).toMap().value(QStringLiteral("entryId")).toString(),
             QStringLiteral("wallpaper"));
    QCOMPARE(childDescriptors.at(4).toMap().value(QStringLiteral("entryId")).toString(),
             QStringLiteral("dock"));
    QCOMPARE(childDescriptors.at(5).toMap().value(QStringLiteral("entryId")).toString(),
             QStringLiteral("animations"));

    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("customization")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("wallpaper")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("dock")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("appearance")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("visual-effects")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("icons")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("animations")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("compositor")), QStringLiteral("compositor"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("missing")), QString());
}

void SettingsNavigationModelTest::startsWithFirstNavigableSidebarDestination()
{
    SettingsNavigationModel model;

    QCOMPARE(model.firstNavigableSidebarDestination(), QStringLiteral("bluetooth"));
}

void SettingsNavigationModelTest::appliesPageAndHubNavigabilityRules()
{
    SettingsNavigationModel model;

    QVERIFY(model.containsNavigableId(QStringLiteral("compositor")));
    QVERIFY(model.containsNavigableId(QStringLiteral("bluetooth")));
    QVERIFY(model.containsNavigableId(QStringLiteral("customization")));
    QVERIFY(model.containsNavigableId(QStringLiteral("wallpaper")));
    QVERIFY(model.containsNavigableId(QStringLiteral("dock")));
    QVERIFY(model.containsNavigableId(QStringLiteral("appearance")));
    QVERIFY(model.containsNavigableId(QStringLiteral("animations")));
    QVERIFY(model.containsNavigableId(QStringLiteral("visual-effects")));
    QVERIFY(model.containsNavigableId(QStringLiteral("icons")));
    QVERIFY(!model.containsNavigableId(QStringLiteral("themes")));
    QVERIFY(!model.containsNavigableId(QStringLiteral("system")));
    QVERIFY(!model.containsNavigableId(QStringLiteral("performance")));
    QVERIFY(!model.containsNavigableId(QStringLiteral("more-settings")));
    QVERIFY(!model.containsNavigableId(QStringLiteral("missing")));

    QVERIFY(model.data(model.index(10, 0), SettingsNavigationModel::EnabledRole).toBool());
    QVERIFY(!model.data(model.index(9, 0), SettingsNavigationModel::EnabledRole).toBool());
    QVERIFY(!model.data(model.index(11, 0), SettingsNavigationModel::EnabledRole).toBool());
}

QTEST_GUILESS_MAIN(SettingsNavigationModelTest)
#include "SettingsNavigationModelTest.moc"
