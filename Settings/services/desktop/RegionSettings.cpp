#include "RegionSettings.hpp"
#include "NativeSettingsSupport.hpp"
#include "RegionCatalog.hpp"
#include "settings/RegionalPreferences.hpp"
#include <QDate>
#include <QLocale>
#include <QMetaType>
#include <QSet>
namespace
{
const QStringList locales = {"en_US", "en_GB", "pt_BR", "es_ES", "fr_FR"};
void validate(const QString &key, const QVariant &value)
{
    if (key == "automatic_location") {
        if (value.metaType().id() != QMetaType::Bool)
            NativeSettings::fail("automatic_location must be a boolean");
    } else {
        if (value.metaType().id() != QMetaType::QString)
            NativeSettings::fail(key + " must be a string");
        const auto text = value.toString();
        if (key == "language" && locales.contains(text))
            return;
        if (key == "country_code" && RegionCatalog::isCountryCode(text))
            return;
        if (key == "time_format" && QStringList{"system", "24h", "12h"}.contains(text))
            return;
        NativeSettings::fail("Unsupported regional value for " + key);
    }
}
} // namespace
namespace RegionSettings
{
QVariantMap read(const QString &path)
{
    auto config = NativeSettings::readObject(path);
    if (!config.contains("language"))
        config["language"] = "en_US";
    if (config.contains("region") && config["region"].metaType().id() != QMetaType::QVariantMap)
        NativeSettings::fail("region must be an object");
    auto region = config["region"].toMap();
    if (!region.contains("country_code"))
        region["country_code"] = "BR";
    if (!region.contains("time_format"))
        region["time_format"] = "system";
    if (!region.contains("automatic_location"))
        region["automatic_location"] = true;
    validate("language", config["language"]);
    for (const auto &key : QStringList{"country_code", "time_format", "automatic_location"})
        validate(key, region[key]);
    config["region"] = region;
    return config;
}
QVariantMap update(const QString &path, const QString &key, const QVariant &value)
{
    validate(key, value);
    NativeSettings::patchObject(path, [&](QVariantMap &raw) {
        // Validate the complete existing document before changing it.
        const auto current = read(path);
        if (key == "language")
            raw[key] = value;
        else {
            auto region = current["region"].toMap();
            region[key] = value;
            raw["region"] = region;
        }
    });
    return read(path);
}
QString effectiveTimeFormat(const QVariantMap &region)
{
    const auto format = region.value("time_format", "system").toString();
    if (format != "system")
        return format;
    return RegionalPreferences::defaultTimeFormat(region.value("country_code").toString());
}
QVariantMap snapshot(const QString &path)
{
    auto config = read(path);
    QVariantList languages;
    const QStringList labels = {"English (United States)", "English (United Kingdom)",
                                "Português (Brasil)", "Español", "Français"};
    for (qsizetype i = 0; i < locales.size(); ++i)
        languages.append(QVariantMap{{"id", locales[i]}, {"name", labels[i]}});
    const auto language = config["language"].toString();
    const auto country = config["region"].toMap()["country_code"].toString();
    const QLocale locale(QLocale(language).language(), QLocale::codeToTerritory(country));
    return {{"config", config},
            {"countries", RegionCatalog::countries(language)},
            {"languages", languages},
            {"effectiveTimeFormat", effectiveTimeFormat(config["region"].toMap())},
            {"formatPreview", QVariantMap{{"date", locale.toString(QDate::currentDate(), QLocale::LongFormat)},
                                          {"number", locale.toString(1234567.89, 'f', 2)}}}};
}
} // namespace RegionSettings
