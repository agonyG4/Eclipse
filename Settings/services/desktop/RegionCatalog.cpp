#include "RegionCatalog.hpp"
#include "RegionCatalogFfi.h"
#include <QCollator>
#include <QDateTime>
#include <QFile>
#include <QLocale>
#include <QMutex>
#include <QTimeZone>
#include <algorithm>
#include <memory>
#include <unicode/locid.h>
#include <unicode/tznames.h>
#include <unicode/uloc.h>
#include <vector>

namespace
{
constexpr int offsetCacheSeconds = 15 * 60;

QString regionText(AstreaRegionText text)
{
    return QString::fromUtf8(text.data, qsizetype(text.size));
}
QByteArray icuLocale(const QString &language)
{
    return language.isEmpty() ? QByteArrayLiteral("en_US") : language.toLatin1();
}
QString displayCountry(const QByteArray &code, const QByteArray &locale)
{
    UErrorCode status = U_ZERO_ERROR;
    UChar buffer[128];
    const auto tag = "und_" + code;
    const auto size = uloc_getDisplayCountry(tag.constData(), locale.constData(), buffer,
                                             std::size(buffer), &status);
    return U_SUCCESS(status) && size > 0
               ? QString::fromUtf16(reinterpret_cast<const char16_t *>(buffer), size)
               : QString();
}
QString offsetLabel(int seconds)
{
    if (seconds == 0)
        return QStringLiteral("UTC");
    const auto minutes = qAbs(seconds) / 60;
    return QStringLiteral("UTC%1%2:%3")
        .arg(seconds < 0 ? QChar(0x2212) : QChar(u'+'))
        .arg(minutes / 60, 2, 10, QChar(u'0'))
        .arg(minutes % 60, 2, 10, QChar(u'0'));
}
QString offsetSearchTerms(int seconds)
{
    const auto minutes = qAbs(seconds) / 60;
    const auto sign = seconds < 0 ? QChar(u'-') : QChar(u'+');
    auto terms = QStringLiteral("utc%1%2:%3\tgmt%1%2\tutc%1%2")
                     .arg(sign)
                     .arg(minutes / 60, 2, 10, QChar(u'0'))
                     .arg(minutes % 60, 2, 10, QChar(u'0'));
    return terms + QStringLiteral("\tutc%1%2").arg(sign).arg(minutes / 60);
}
struct RawZone {
    QString country;
    QString id;
    QString comment;
};
std::vector<RawZone> readZones(const QString &tablePath)
{
    std::vector<RawZone> zones;
    QFile file(tablePath);
    if (file.open(QIODevice::ReadOnly)) {
        const auto bytes = file.readAll();
        astrea_region_parse_zones(
            astreaRegionText(bytes),
            [](void *context, const AstreaRegionZone *zone) {
                static_cast<std::vector<RawZone> *>(context)->push_back(
                    {regionText(zone->country), regionText(zone->id), regionText(zone->comment)});
            },
            &zones);
    }
    return zones;
}
// Building the list formats ~420 zones through ICU, while every saved setting
// rebuilds the snapshot. Offsets only move at transitions, so a short cache is safe.
struct ZoneCache {
    QString key;
    QDateTime builtAt;
    QVariantList zones;
};
QMutex zoneCacheLock;
ZoneCache zoneCache;
} // namespace

namespace RegionCatalog
{
QString zoneTablePath()
{
    const auto directory = qEnvironmentVariable("TZDIR", QStringLiteral("/usr/share/zoneinfo"));
    return directory + QStringLiteral("/zone.tab");
}
bool isCountryCode(const QString &code)
{
    return code.size() == 2 && code[0].isUpper() && code[1].isUpper() &&
           QLocale::codeToTerritory(code) != QLocale::AnyTerritory;
}
bool isTimeZoneId(const QString &id)
{
    return !id.isEmpty() && QTimeZone::isTimeZoneIdAvailable(id.toLatin1());
}
QString countryName(const QString &code, const QString &language)
{
    const auto name = displayCountry(code.toLatin1(), icuLocale(language));
    return name.isEmpty() ? code : name;
}
QString fold(const QString &text)
{
    QString folded;
    const auto decomposed = text.normalized(QString::NormalizationForm_KD);
    folded.reserve(decomposed.size());
    for (const auto character : decomposed) {
        const auto category = character.category();
        if (category != QChar::Mark_NonSpacing && category != QChar::Mark_Enclosing)
            folded += character.toCaseFolded();
    }
    return folded;
}
QVariantList countries(const QString &language)
{
    const auto locale = icuLocale(language);
    QCollator collator{QLocale(language)};
    struct Row {
        QString name;
        QVariantMap value;
    };
    std::vector<Row> rows;
    for (auto iso = uloc_getISOCountries(); iso && *iso; ++iso) {
        const auto code = QString::fromLatin1(*iso);
        if (!isCountryCode(code))
            continue;
        const auto name = displayCountry(*iso, locale);
        if (name.isEmpty())
            continue;
        const auto english = displayCountry(*iso, "en");
        const auto native =
            QLocale(QLocale::AnyLanguage, QLocale::codeToTerritory(code)).nativeTerritoryName();
        const auto tag = "und_" + QByteArray(*iso);
        // Common abbreviations ICU has no field for.
        const auto alias = code == u"GB"   ? QStringLiteral("uk")
                           : code == u"US" ? QStringLiteral("eua")
                                           : QString();
        const QStringList fields{name,
                                 english,
                                 native,
                                 code,
                                 QString::fromLatin1(uloc_getISO3Country(tag.constData())),
                                 alias};
        rows.push_back({name,
                        {{"id", code},
                         {"name", name},
                         {"subtitle", native != name ? native : QString()},
                         {"trailing", code},
                         {"searchKey", fold(fields.join(u'\t'))}}});
    }
    std::sort(rows.begin(), rows.end(), [&](const Row &left, const Row &right) {
        return collator.compare(left.name, right.name) < 0;
    });
    QVariantList result;
    result.reserve(qsizetype(rows.size()));
    for (const auto &row : rows)
        result.append(row.value);
    return result;
}
QVariantList timeZones(const QString &language, const QString &systemZone, const QString &tablePath)
{
    const auto now = QDateTime::currentDateTimeUtc();
    const auto cacheKey = language + u'\n' + systemZone + u'\n' + tablePath;
    QMutexLocker lock(&zoneCacheLock);
    if (zoneCache.key == cacheKey && zoneCache.builtAt.secsTo(now) < offsetCacheSeconds)
        return zoneCache.zones;

    auto raw = readZones(tablePath);
    raw.push_back({{}, QStringLiteral("UTC"), {}});
    const auto listed = [&](const QString &id) {
        return std::any_of(raw.begin(), raw.end(), [&](const RawZone &zone) { return zone.id == id; });
    };
    if (raw.size() == 1) {
        // Without zone.tab the zones are still selectable, only without a country.
        for (const auto &id : QTimeZone::availableTimeZoneIds())
            if (!listed(QString::fromLatin1(id)))
                raw.push_back({{}, QString::fromLatin1(id), {}});
    }
    if (!systemZone.isEmpty() && !listed(systemZone))
        raw.push_back({{}, systemZone, {}});

    const QLocale qtLocale(language);
    const auto locale = icuLocale(language);
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::TimeZoneNames> names(
        icu::TimeZoneNames::createInstance(icu::Locale(locale.constData()), status));
    if (U_FAILURE(status))
        names.reset();
    QCollator collator{qtLocale};
    struct Row {
        int offset;
        QString name;
        QVariantMap value;
    };
    std::vector<Row> rows;
    rows.reserve(raw.size());
    for (const auto &zone : raw) {
        const QTimeZone timeZone(zone.id.toLatin1());
        if (!timeZone.isValid())
            continue;
        const auto offset = timeZone.offsetFromUtc(now);
        auto fallback = zone.id.section(u'/', -1);
        fallback.replace(u'_', u' ');
        QString city;
        if (names) {
            icu::UnicodeString exemplar;
            names->getExemplarLocationName(icu::UnicodeString::fromUTF8(zone.id.toStdString()), exemplar);
            if (!exemplar.isBogus() && !exemplar.isEmpty())
                city = QString::fromUtf16(reinterpret_cast<const char16_t *>(exemplar.getBuffer()),
                                          exemplar.length());
        }
        if (city.isEmpty())
            city = fallback;
        const auto description = timeZone.displayName(now, QTimeZone::LongName, qtLocale);
        const auto country = zone.country.isEmpty() ? QString() : countryName(zone.country, language);
        const auto subtitle = country.isEmpty() ? description
                                                : description + QStringLiteral(" · ") + country;
        auto idWords = zone.id;
        idWords.replace(u'/', u' ').replace(u'_', u' ');
        const QStringList fields{city,   fallback,     idWords,
                                 country, description, zone.comment,
                                 offsetSearchTerms(offset)};
        rows.push_back({offset,
                        city,
                        {{"id", zone.id},
                         {"name", city},
                         {"subtitle", subtitle},
                         {"trailing", offsetLabel(offset)},
                         {"country", zone.country},
                         {"searchKey", fold(fields.join(u'\t'))}}});
    }
    std::sort(rows.begin(), rows.end(), [&](const Row &left, const Row &right) {
        return left.offset != right.offset ? left.offset < right.offset
                                           : collator.compare(left.name, right.name) < 0;
    });
    QVariantList result;
    result.reserve(qsizetype(rows.size()));
    for (const auto &row : rows)
        result.append(row.value);
    zoneCache = {cacheKey, now, result};
    return result;
}
ZoneMatch nearestZone(double latitude, double longitude, const QString &tablePath)
{
    ZoneMatch match;
    QFile file(tablePath);
    if (!file.open(QIODevice::ReadOnly))
        return match;
    const auto bytes = file.readAll();
    astrea_region_nearest_zone(
        astreaRegionText(bytes), latitude, longitude,
        [](void *context, const AstreaRegionZone *zone) {
            *static_cast<ZoneMatch *>(context) = {regionText(zone->country), regionText(zone->id)};
        },
        &match);
    return match;
}
QVariantList withLetterHeadings(const QVariantList &items)
{
    QVariantList result;
    result.reserve(items.size());
    QString previous;
    for (const auto &item : items) {
        auto entry = item.toMap();
        const auto letter = fold(entry.value(QStringLiteral("name")).toString()).left(1).toUpper();
        if (letter != previous) {
            entry[QStringLiteral("heading")] = letter;
            previous = letter;
        }
        result.append(entry);
    }
    return result;
}
QVariantList filter(const QVariantList &items, const QString &query)
{
    if (query.trimmed().isEmpty())
        return items;
    std::vector<QByteArray> storage;
    std::vector<AstreaRegionText> keys;
    storage.reserve(std::size_t(items.size()));
    keys.reserve(std::size_t(items.size()));
    for (const auto &item : items) {
        storage.push_back(item.toMap().value(QStringLiteral("searchKey")).toString().toUtf8());
        keys.push_back(astreaRegionText(storage.back()));
    }
    const auto folded = fold(query).toUtf8();
    std::vector<std::size_t> order(keys.size());
    const auto count = astrea_region_rank(astreaRegionText(folded), keys.data(), keys.size(), order.data());
    QVariantList result;
    result.reserve(qsizetype(count));
    for (std::size_t index = 0; index < count; ++index)
        result.append(items[qsizetype(order[index])]);
    return result;
}
} // namespace RegionCatalog
