#ifndef ASTREA_APP_TILES_H
#define ASTREA_APP_TILES_H

/* C ABI of shared/tiles (Rust): macOS Tahoe tiles for app icons. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Edge of the square masters astrea_app_tiles_render writes (512). */
uint32_t astrea_app_tiles_master_extent(void);

/* Bumped whenever a tile would look different; key caches with it. */
uint32_t astrea_app_tiles_version(void);

/* Builds both theme variants from straight-alpha RGBA8 artwork. `rgba` holds
 * `height` rows of `stride` bytes; `light_out` and `dark_out` each receive
 * extent * extent * 4 bytes of straight RGBA8. Returns 1 on success. */
int32_t astrea_app_tiles_render(const uint8_t *rgba, uint32_t width, uint32_t height,
                                uint32_t stride, int32_t keep_dark,
                                uint8_t *light_out, uint8_t *dark_out);

/* Rasterises SVG or SVGZ artwork (`len` bytes) to fit an extent x extent
 * square, centred, into `out` (extent * extent * 4 bytes, straight RGBA8).
 * Returns 1 on success. Unlike Qt's SVG Tiny reader it honours clip paths,
 * masks and filters. */
int32_t astrea_app_tiles_rasterize_svg(const uint8_t *data, size_t len, uint32_t extent,
                                       uint8_t *out);

/* Bumped whenever a style would look different; key caches with it. */
uint32_t astrea_app_tiles_style_version(void);

/* Restyles one finished extent x extent app icon (straight RGBA8, `stride`
 * bytes per row) into `out` (extent * extent * 4 bytes, straight RGBA8).
 * style: 1 dark, 2 clear, 3 tinted; dark != 0 picks the style's dark-mode
 * form; tint is 0xRRGGBB. Returns 1 on success. */
int32_t astrea_app_tiles_style(const uint8_t *rgba, uint32_t width, uint32_t height,
                               uint32_t stride, uint32_t style, int32_t dark,
                               uint32_t tint, uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif
