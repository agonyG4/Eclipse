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
    return {
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
    };
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
                    SettingsNavigationEntry::Kind::Group),
          makeEntry(QStringLiteral("appearance"), QStringLiteral("Appearance"), {},
                    QStringLiteral("Appearance settings"), {}, QStringLiteral("theme"), {},
                    SettingsNavigationEntry::Kind::Group),
          makeEntry(QStringLiteral("more-settings"), QStringLiteral("More Settings"), {},
                    QStringLiteral("Additional settings"), QStringLiteral("\uf013"), {}, {},
                    SettingsNavigationEntry::Kind::Group),
      }
{
}

const QVector<SettingsNavigationEntry> &SettingsNavigationCatalog::entries() const
{
    return m_entries;
}
