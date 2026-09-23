#include "core/navigation/SettingsNavigationCatalog.hpp"

namespace {

SettingsNavigationEntry makeEntry(const QString &id,
                                  const QString &label,
                                  const QString &labelKey,
                                  const QString &subtitle,
                                  const QString &sym,
                                  const QString &iconKey,
                                  const QUrl &pageSource = {},
                                  SettingsNavigationEntry::Kind kind = SettingsNavigationEntry::Kind::Page,
                                  bool enabled = true,
                                  bool sidebarVisible = true,
                                  const QString &parentId = {},
                                  const QString &subtitleKey = {})
{
    SettingsNavigationEntry entry{
        id,
        label,
        labelKey,
        subtitle,
        subtitleKey,
        sym,
        {},
        iconKey,
        pageSource,
        kind,
        enabled,
        sidebarVisible,
        parentId,
    };
    return entry;
}

} // namespace

SettingsNavigationCatalog::SettingsNavigationCatalog()
    : m_entries{
          makeEntry(QStringLiteral("system"), QStringLiteral("System"),
                    QStringLiteral("settings.nav.system"), QStringLiteral("System information"),
                    QStringLiteral("\uf303"), {}),
          makeEntry(QStringLiteral("software-update"), QStringLiteral("Software Update"),
                    QStringLiteral("settings.nav.software_update"), QStringLiteral("System updates"),
                    {}, QStringLiteral("software-center")),
          makeEntry(QStringLiteral("internet"), QStringLiteral("Internet"),
                    QStringLiteral("settings.nav.internet"), QStringLiteral("Network connections"),
                    {}, QStringLiteral("network")),
          makeEntry(QStringLiteral("bluetooth"), QStringLiteral("Bluetooth"),
                    QStringLiteral("settings.nav.bluetooth"), QStringLiteral("Bluetooth devices"),
                    {}, QStringLiteral("bluetooth"),
                    QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Bluetooth.qml"))),
          makeEntry(QStringLiteral("audio"), QStringLiteral("Audio"),
                    QStringLiteral("settings.nav.audio"), QStringLiteral("Sound and volume"),
                    {}, QStringLiteral("audio")),
          makeEntry(QStringLiteral("components"), QStringLiteral("Components"), {},
                    QStringLiteral("Astrea shell components"), QStringLiteral("\uf0e8"), {}),
          makeEntry(QStringLiteral("services"), QStringLiteral("Services"), {},
                    QStringLiteral("Astrea background services"), QStringLiteral("\uf085"), {}),
          makeEntry(QStringLiteral("compositor"), QStringLiteral("Compositor"),
                    QStringLiteral("settings.nav.compositor"), QStringLiteral("Astrea compositor preferences"),
                    QStringLiteral("\uf2d0"), {},
                    QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Compositor.qml"))),
          makeEntry({}, {}, {}, {}, {}, {}, {}, SettingsNavigationEntry::Kind::Spacer, false),
          makeEntry(QStringLiteral("performance"), QStringLiteral("Performance"), {},
                    QStringLiteral("Performance settings"), {}, QStringLiteral("performance"),
                    QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/navigation/Hub.qml")),
                    SettingsNavigationEntry::Kind::Hub),
          makeEntry(QStringLiteral("customization"), QStringLiteral("Customization"),
                    QStringLiteral("settings.nav.customization"),
                    QStringLiteral("Personalize the look and behavior of your Astrea desktop."), {},
                    QStringLiteral("theme"),
                    QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/navigation/Hub.qml")),
                    SettingsNavigationEntry::Kind::Hub, true, true, {},
                    QStringLiteral("settings.nav.customization.subtitle")),
          makeEntry(QStringLiteral("appearance"), QStringLiteral("Appearance"),
                    QStringLiteral("settings.nav.appearance"), QStringLiteral("System theme, appearance, and accent"),
                    {}, QStringLiteral("theme"),
                    QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Appearance.qml")),
                    SettingsNavigationEntry::Kind::Page, true, false, QStringLiteral("customization"),
                    QStringLiteral("settings.nav.appearance.subtitle")),
          makeEntry(QStringLiteral("visual-effects"), QStringLiteral("Visual Effects"),
                    QStringLiteral("settings.nav.visual_effects"), QStringLiteral("Interface material style"),
                    {}, QStringLiteral("theme"),
                    QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/VisualEffects.qml")),
                    SettingsNavigationEntry::Kind::Page, true, false, QStringLiteral("customization")),
          makeEntry(QStringLiteral("icons"), QStringLiteral("Icons"),
                    QStringLiteral("settings.nav.icons"), QStringLiteral("Installed icon themes"),
                    {}, QStringLiteral("theme"),
                    QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Icons.qml")),
                    SettingsNavigationEntry::Kind::Page, true, false, QStringLiteral("customization")),
          makeEntry(QStringLiteral("wallpaper"), QStringLiteral("Wallpaper"),
                    QStringLiteral("settings.nav.wallpaper"), QStringLiteral("Desktop background"),
                    {}, QStringLiteral("wallpaper"),
                    QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Wallpaper.qml")),
                    SettingsNavigationEntry::Kind::Page, true, false, QStringLiteral("customization")),
          makeEntry(QStringLiteral("dock"), QStringLiteral("Dock"),
                    QStringLiteral("settings.nav.dock"), QStringLiteral("Dock layout and behavior"),
                    {}, QStringLiteral("theme"),
                    QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Dock.qml")),
                    SettingsNavigationEntry::Kind::Page, true, false, QStringLiteral("customization")),
          makeEntry(QStringLiteral("animations"), QStringLiteral("Animations"),
                    QStringLiteral("settings.nav.animations"), QStringLiteral("Motion and transition behavior"),
                    {}, QStringLiteral("animations"),
                    QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Animations.qml")),
                    SettingsNavigationEntry::Kind::Page, true, false, QStringLiteral("customization")),
          makeEntry(QStringLiteral("more-settings"), QStringLiteral("More Settings"), {},
                    QStringLiteral("Additional settings"), QStringLiteral("\uf013"), {},
                    QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/navigation/Hub.qml")),
                    SettingsNavigationEntry::Kind::Hub),
      }
{
}

const QVector<SettingsNavigationEntry> &SettingsNavigationCatalog::entries() const
{
    return m_entries;
}
