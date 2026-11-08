#pragma once

#include <cstddef>
#include <cstdint>

struct AstreaIconDirectory {
    std::int32_t size;
    std::int32_t scale;
    std::int32_t minSize;
    std::int32_t maxSize;
    std::int32_t threshold;
    std::uint8_t type; // Fixed, Scalable, Threshold, in this order.
};

// The callback is synchronous. A successful lookup publishes the first
// available asset in that directory, in the caller's root/extension order.
extern "C" bool astrea_icon_choose_directory(
    const AstreaIconDirectory *directories, std::size_t count,
    std::int32_t size, std::int32_t scale,
    bool (*lookup)(void *, std::size_t), void *context);

enum AstreaIconThemeSource : std::uint8_t {
    AstreaIconThemeFallback,
    AstreaIconThemeEnvironment,
    AstreaIconThemeConfig,
    AstreaIconThemeCompatibility,
    AstreaIconThemePlatform,
};
enum AstreaIconVariant : std::uint8_t {
    AstreaIconBase = 1,
    AstreaIconLight,
    AstreaIconDark,
};
using AstreaIconThemeLookup = bool (*)(void *, std::uint8_t);

// The lookup is synchronous and must publish the candidate before returning true.
extern "C" bool astrea_icon_choose_variant(const char *name, std::size_t length, bool light,
                                           AstreaIconThemeLookup lookup, void *context);
extern "C" std::uint8_t astrea_icon_choose_theme(bool platformIsHicolor,
                                                 AstreaIconThemeLookup lookup, void *context);

struct AstreaIconText {
    const std::uint16_t *data;
    std::size_t size;
};
using AstreaIconThemeChild = void (*)(void *, AstreaIconText);
using AstreaIconThemeVisit = void (*)(void *, AstreaIconText, AstreaIconThemeChild, void *);

// Visits each valid theme once. The visitor supplies inherited names synchronously.
extern "C" void astrea_icon_walk_themes(
    const AstreaIconText *roots, std::size_t count, bool deferInheritedHicolor,
    AstreaIconThemeVisit visit, void *context);
extern "C" void astrea_icon_theme_family(
    AstreaIconText theme, AstreaIconText fallback, AstreaIconThemeVisit visit, void *context);
extern "C" std::uint8_t astrea_icon_directory_type(
    AstreaIconText type, bool validSize, std::int32_t size, std::int32_t scale,
    std::int32_t minSize, std::int32_t maxSize, std::int32_t threshold);
extern "C" bool astrea_icon_valid_theme(AstreaIconText name);
extern "C" bool astrea_icon_safe_relative_path(AstreaIconText path, bool absolute);

enum AstreaIconIndexResult : std::uint8_t {
    AstreaIconIndexInvalid,
    AstreaIconIndexParsed,
    AstreaIconIndexNativeEncoding,
};
// A null directory denotes an inherited theme name. All views are call-scoped.
using AstreaIconIndexEntry = void (*)(void *, AstreaIconText, const AstreaIconDirectory *);
extern "C" std::uint8_t astrea_icon_parse_index(const char *data, std::size_t size,
                                               AstreaIconIndexEntry entry, void *context);
