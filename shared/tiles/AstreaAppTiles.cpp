#include "AstreaAppTiles.hpp"

#include "icons/AppTileHook.hpp"

#include <astrea_app_tiles.h>

namespace AstreaTiles {
namespace {

Masters renderWithRust(const QImage &artwork, bool keepDark)
{
    const QImage source = artwork.convertToFormat(QImage::Format_RGBA8888);
    if (source.isNull())
        return {};
    const int extent = int(astrea_app_tiles_master_extent());
    QImage light(extent, extent, QImage::Format_RGBA8888);
    QImage dark(extent, extent, QImage::Format_RGBA8888);
    // The C ABI writes tightly packed rows; a 512 px RGBA8888 QImage has no
    // row padding.
    if (light.bytesPerLine() != extent * 4 || dark.bytesPerLine() != extent * 4)
        return {};
    const int ok = astrea_app_tiles_render(source.constBits(), uint32_t(source.width()),
                                           uint32_t(source.height()),
                                           uint32_t(source.bytesPerLine()), keepDark ? 1 : 0,
                                           light.bits(), dark.bits());
    if (ok != 1)
        return {};
    return {light, dark};
}

} // namespace

void installRustRenderer()
{
    Renderer renderer;
    renderer.render = renderWithRust;
    renderer.masterExtent = int(astrea_app_tiles_master_extent());
    renderer.version = int(astrea_app_tiles_version());
    setRenderer(renderer);
}

} // namespace AstreaTiles
