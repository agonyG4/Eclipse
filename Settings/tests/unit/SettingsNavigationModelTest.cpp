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
    void exposesSevenVisualMigrationRoutes();
    void startsWithFirstNavigableSidebarDestination();
    void appliesPageAndHubNavigabilityRules();
};

void SettingsNavigationModelTest::exposesExactSidebarOrder()
{
    SettingsNavigationModel model;
    const QStringList expected{
        QStringLiteral("system"), QStringLiteral("software-update"), QStringLiteral("internet"),
        QStringLiteral("bluetooth"), QStringLiteral("devices"), QStringLiteral("audio"), QStringLiteral("display"),
        QStringLiteral("components"),
        QStringLiteral("services"), QStringLiteral("compositor"), QString(),
        QStringLiteral("performance"), QStringLiteral("customization"), QStringLiteral("more-settings"),
    };

    QCOMPARE(model.rowCount(), 14);
    QCOMPARE(model.rowCount(), expected.size());
    for (int row = 0; row < expected.size(); ++row)
        QCOMPARE(model.get(row).value(QStringLiteral("entryId")).toString(), expected.at(row));
}

void SettingsNavigationModelTest::exposesCanonicalKindsAndVisibility()
{
    SettingsNavigationModel model;
    const QStringList expectedKinds{
        QStringLiteral("page"), QStringLiteral("page"), QStringLiteral("page"),
        QStringLiteral("page"),
        QStringLiteral("page"), QStringLiteral("page"), QStringLiteral("page"),
        QStringLiteral("page"), QStringLiteral("page"), QStringLiteral("page"), QStringLiteral("spacer"),
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
                              QStringLiteral("wallpaper"), QStringLiteral("dock"),
                              QStringLiteral("animations"), QStringLiteral("screenshots")}) {
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
    const QModelIndex spacer = model.index(10, 0);
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
    const QUrl screenshotsRoute(
        QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Screenshots.qml"));

    QCOMPARE(model.pageSourceForId(QStringLiteral("compositor")), compositorRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("internet")),
             QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Internet.qml")));
    QCOMPARE(model.pageSourceForId(QStringLiteral("bluetooth")), bluetoothRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("devices")),
             QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Devices.qml")));
    QCOMPARE(model.pageSourceForId(QStringLiteral("audio")),
             QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Audio.qml")));
    QCOMPARE(model.pageSourceForId(QStringLiteral("display")),
             QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Display.qml")));
    const QVariantMap display = model.descriptorForId(QStringLiteral("display"));
    QCOMPARE(display.value(QStringLiteral("sym")).toString(), QString(QChar(0xea89)));
    QCOMPARE(display.value(QStringLiteral("iconKey")).toString(), QStringLiteral("display"));
    QCOMPARE(display.value(QStringLiteral("subtitleKey")).toString(),
             QStringLiteral("settings.nav.display.subtitle"));
    QCOMPARE(model.pageSourceForId(QStringLiteral("customization")), customizationRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("wallpaper")), wallpaperRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("dock")), dockRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("appearance")), appearanceRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("animations")), animationsRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("visual-effects")), visualEffectsRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("icons")), iconsRoute);
    QCOMPARE(model.pageSourceForId(QStringLiteral("screenshots")), screenshotsRoute);
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
    QCOMPARE(children.size(), 8);
    QCOMPARE(children.at(0).id, QStringLiteral("appearance"));
    QCOMPARE(children.at(1).id, QStringLiteral("visual-effects"));
    QCOMPARE(children.at(2).id, QStringLiteral("icons"));
    QCOMPARE(children.at(3).id, QStringLiteral("wallpaper"));
    QCOMPARE(children.at(4).id, QStringLiteral("dock"));
    QCOMPARE(children.at(5).id, QStringLiteral("animations"));
    QCOMPARE(children.at(6).id, QStringLiteral("screenshots"));
    QCOMPARE(children.at(7).id, QStringLiteral("island"));

    const QVariantList childDescriptors = model.childDescriptorsForId(QStringLiteral("customization"));
    QCOMPARE(childDescriptors.size(), 8);
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
    QCOMPARE(childDescriptors.at(6).toMap().value(QStringLiteral("entryId")).toString(),
             QStringLiteral("screenshots"));
    QCOMPARE(childDescriptors.at(7).toMap().value(QStringLiteral("entryId")).toString(),
             QStringLiteral("island"));

    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("customization")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("wallpaper")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("dock")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("appearance")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("visual-effects")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("icons")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("animations")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("screenshots")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("island")), QStringLiteral("customization"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("compositor")), QStringLiteral("compositor"));
    QCOMPARE(model.sidebarAncestorForId(QStringLiteral("missing")), QString());
}

void SettingsNavigationModelTest::exposesSevenVisualMigrationRoutes()
{
    SettingsNavigationModel model;
    const struct RouteExpectation {
        const char *id;
        const char *parent;
        const char *label;
        const char *source;
    } expected[] = {
        {"performance-settings", "performance", "Performance", "system/Performance.qml"},
        {"proton", "performance", "Proton", "gaming/Proton.qml"},
        {"compatibility", "more-settings", "Compatibility", "gaming/Compatibility.qml"},
        {"language", "more-settings", "Language", "system/Language.qml"},
        {"apps", "more-settings", "Apps", "apps/Applications.qml"},
        {"island", "customization", "Island", "appearance/Island.qml"},
        {"user-profile", "more-settings", "User Profile", "personalization/UserProfile.qml"},
    };
    for (const auto &route : expected) {
        const QString id = QString::fromLatin1(route.id);
        const QVariantMap descriptor = model.descriptorForId(id);
        QVERIFY2(!descriptor.isEmpty(), route.id);
        QCOMPARE(descriptor.value(QStringLiteral("label")).toString(), QString::fromLatin1(route.label));
        QCOMPARE(descriptor.value(QStringLiteral("parentId")).toString(), QString::fromLatin1(route.parent));
        QVERIFY(!descriptor.value(QStringLiteral("sidebarVisible")).toBool());
        QVERIFY(descriptor.value(QStringLiteral("entryEnabled")).toBool());
        QVERIFY(model.containsNavigableId(id));
        QVERIFY(model.pageSourceForId(id).toString().endsWith(QString::fromLatin1(route.source)));
        QCOMPARE(model.sidebarAncestorForId(id), QString::fromLatin1(route.parent));
    }
    const auto performanceChildren = model.childDescriptorsForId(QStringLiteral("performance"));
    QCOMPARE(performanceChildren.size(), 2);
    QCOMPARE(performanceChildren.at(0).toMap().value(QStringLiteral("entryId")).toString(),
             QStringLiteral("performance-settings"));
    QCOMPARE(performanceChildren.at(1).toMap().value(QStringLiteral("entryId")).toString(),
             QStringLiteral("proton"));
    const auto moreChildren = model.childDescriptorsForId(QStringLiteral("more-settings"));
    QCOMPARE(moreChildren.size(), 4);
    QCOMPARE(moreChildren.at(0).toMap().value(QStringLiteral("entryId")).toString(), QStringLiteral("compatibility"));
    QCOMPARE(moreChildren.at(1).toMap().value(QStringLiteral("entryId")).toString(), QStringLiteral("language"));
    QCOMPARE(moreChildren.at(2).toMap().value(QStringLiteral("entryId")).toString(), QStringLiteral("apps"));
    QCOMPARE(moreChildren.at(3).toMap().value(QStringLiteral("entryId")).toString(), QStringLiteral("user-profile"));
}

void SettingsNavigationModelTest::startsWithFirstNavigableSidebarDestination()
{
    SettingsNavigationModel model;

    QCOMPARE(model.firstNavigableSidebarDestination(), QStringLiteral("internet"));
}

void SettingsNavigationModelTest::appliesPageAndHubNavigabilityRules()
{
    SettingsNavigationModel model;

    QVERIFY(model.containsNavigableId(QStringLiteral("compositor")));
    QVERIFY(model.containsNavigableId(QStringLiteral("bluetooth")));
    QVERIFY(model.containsNavigableId(QStringLiteral("audio")));
    QVERIFY(model.containsNavigableId(QStringLiteral("display")));
    QVERIFY(model.containsNavigableId(QStringLiteral("customization")));
    QVERIFY(model.containsNavigableId(QStringLiteral("wallpaper")));
    QVERIFY(model.containsNavigableId(QStringLiteral("dock")));
    QVERIFY(model.containsNavigableId(QStringLiteral("appearance")));
    QVERIFY(model.containsNavigableId(QStringLiteral("animations")));
    QVERIFY(model.containsNavigableId(QStringLiteral("visual-effects")));
    QVERIFY(model.containsNavigableId(QStringLiteral("icons")));
    QVERIFY(model.containsNavigableId(QStringLiteral("screenshots")));
    QVERIFY(!model.containsNavigableId(QStringLiteral("themes")));
    QVERIFY(!model.containsNavigableId(QStringLiteral("system")));
    QVERIFY(model.containsNavigableId(QStringLiteral("performance")));
    QVERIFY(model.containsNavigableId(QStringLiteral("more-settings")));
    QVERIFY(!model.containsNavigableId(QStringLiteral("missing")));

    QVERIFY(model.data(model.index(11, 0), SettingsNavigationModel::EnabledRole).toBool());
    QVERIFY(!model.data(model.index(10, 0), SettingsNavigationModel::EnabledRole).toBool());
    QVERIFY(model.data(model.index(12, 0), SettingsNavigationModel::EnabledRole).toBool());
}

QTEST_GUILESS_MAIN(SettingsNavigationModelTest)
#include "SettingsNavigationModelTest.moc"
