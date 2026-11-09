#pragma once
// C ABI of Settings/backend/src/region. All views are call-scoped UTF-8.
#include <QByteArray>
#include <cstddef>

struct AstreaRegionText {
    const char *data;
    std::size_t size;
};
inline AstreaRegionText astreaRegionText(const QByteArray &bytes)
{
    return {bytes.constData(), std::size_t(bytes.size())};
}
struct AstreaRegionZone {
    AstreaRegionText country;
    AstreaRegionText id;
    AstreaRegionText comment;
    double latitude;
    double longitude;
};
using AstreaRegionZoneCallback = void (*)(void *, const AstreaRegionZone *);
using AstreaRegionIpCallback = void (*)(void *, AstreaRegionText, AstreaRegionText);
extern "C" std::size_t astrea_region_parse_zones(AstreaRegionText table,
                                                 AstreaRegionZoneCallback emit, void *context);
extern "C" bool astrea_region_nearest_zone(AstreaRegionText table, double latitude,
                                           double longitude, AstreaRegionZoneCallback emit,
                                           void *context);
extern "C" std::size_t astrea_region_rank(AstreaRegionText query, const AstreaRegionText *keys,
                                          std::size_t count, std::size_t *out);
extern "C" unsigned char astrea_region_parse_ip_lookup(AstreaRegionText body,
                                                       AstreaRegionIpCallback emit, void *context);
