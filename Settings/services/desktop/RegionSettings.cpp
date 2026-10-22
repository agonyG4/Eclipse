#include "RegionSettings.hpp"
#include "NativeSettingsSupport.hpp"
#include <QMetaType>
#include <QSet>
namespace
{
const QStringList codes = {"US", "GB", "BR", "CA", "FR", "DE", "JP",
                           "PT", "ES", "IT", "AR", "CL", "UY"};
const QStringList names = {"United States", "United Kingdom", "Brazil",   "Canada", "France",
                           "Germany",       "Japan",          "Portugal", "Spain",  "Italy",
                           "Argentina",     "Chile",          "Uruguay"};
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
        if (key == "country_code" && codes.contains(text))
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
    return QStringList{"US", "CA"}.contains(region.value("country_code").toString()) ? "12h"
                                                                                     : "24h";
}
QVariantMap snapshot(const QString &path)
{
    auto config = read(path);
    QVariantList countries, languages;
    for (qsizetype i = 0; i < codes.size(); ++i)
        countries.append(QVariantMap{{"id", codes[i]}, {"name", names[i]}});
    const QStringList labels = {"English (United States)", "English (United Kingdom)",
                                "Português (Brasil)", "Español", "Français"};
    for (qsizetype i = 0; i < locales.size(); ++i)
        languages.append(QVariantMap{{"id", locales[i]}, {"name", labels[i]}});
    return {{"config", config},
            {"countries", countries},
            {"languages", languages},
            {"effectiveTimeFormat", effectiveTimeFormat(config["region"].toMap())}};
}
} // namespace RegionSettings
