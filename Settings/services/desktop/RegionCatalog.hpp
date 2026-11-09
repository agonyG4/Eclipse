#pragma once
#include <QString>
#include <QVariantList>

// The complete country and time-zone lists, from ICU and the system's tzdata.
namespace RegionCatalog
{
struct ZoneMatch {
    QString country;
    QString zone;
};
// Honours TZDIR, otherwise /usr/share/zoneinfo.
QString zoneTablePath();
bool isCountryCode(const QString &code);
bool isTimeZoneId(const QString &id);
QString countryName(const QString &code, const QString &language);
QVariantList countries(const QString &language);
QVariantList timeZones(const QString &language, const QString &systemZone,
                       const QString &tablePath = zoneTablePath());
// Empty when coordinates are invalid or tzdata cannot be read.
ZoneMatch nearestZone(double latitude, double longitude, const QString &tablePath = zoneTablePath());
QString fold(const QString &text);
// Entries of `items` matching `query`, best first. Each item has a `searchKey`.
QVariantList filter(const QVariantList &items, const QString &query);
// Marks each item that starts a new initial, folded so "Á" files under "A".
QVariantList withLetterHeadings(const QVariantList &items);
} // namespace RegionCatalog
