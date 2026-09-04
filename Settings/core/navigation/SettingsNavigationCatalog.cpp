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
                                  bool enabled = true)
{
    SettingsNavigationEntry entry{
        id,
        label,
        labelKey,
        subtitle,
        sym,
        {},
        iconKey,
        pageSource,
        kind,
        enabled,
        {},
        {},
        kind == SettingsNavigationEntry::Kind::Section,
    };
    if (kind == SettingsNavigationEntry::Kind::Section)
        entry.sectionKey = id;
    return entry;
}

SettingsNavigationEntry makeChild(const QString &id,
                                   const QString &label,
                                   const QString &labelKey,
                                   const QString &subtitle,
                                   const QString &iconKey,
                                   const QUrl &pageSource,
                                   const QString &parentSection)
{
    SettingsNavigationEntry entry = makeEntry(id, label, labelKey, subtitle, {}, iconKey,
                                              pageSource, SettingsNavigationEntry::Kind::Child);
    entry.parentSection = parentSection;
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
                    {}, QStringLiteral("bluetooth")),
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
                    QStringLiteral("Performance settings"), {}, QStringLiteral("performance"), {},
                    SettingsNavigationEntry::Kind::Section),
          makeEntry(QStringLiteral("appearance"), QStringLiteral("Appearance"), {},
                    QStringLiteral("Appearance settings"), {}, QStringLiteral("theme"), {},
                    SettingsNavigationEntry::Kind::Section),
          makeChild(QStringLiteral("wallpaper"), QStringLiteral("Wallpaper"),
                    QStringLiteral("settings.nav.wallpaper"), QStringLiteral("Desktop background"),
                    QStringLiteral("wallpaper"),
                    QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Wallpaper.qml")),
                    QStringLiteral("appearance")),
          makeChild(QStringLiteral("dock"), QStringLiteral("Dock"),
                    QStringLiteral("settings.nav.dock"), QStringLiteral("Dock layout and behavior"),
                    QStringLiteral("theme"),
                    QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Dock.qml")),
                    QStringLiteral("appearance")),
          makeEntry(QStringLiteral("more-settings"), QStringLiteral("More Settings"), {},
                    QStringLiteral("Additional settings"), QStringLiteral("\uf013"), {}, {},
                    SettingsNavigationEntry::Kind::Section),
      }
{
}

const QVector<SettingsNavigationEntry> &SettingsNavigationCatalog::entries() const
{
    return m_entries;
}
