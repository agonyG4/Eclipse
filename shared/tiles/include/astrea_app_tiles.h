#ifndef ASTREA_APP_TILES_H
#define ASTREA_APP_TILES_H

/* C ABI of shared/tiles (Rust): macOS Tahoe tiles for app icons. */

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

#ifdef __cplusplus
}
#endif

#endif
