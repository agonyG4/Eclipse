#include "ContextMenuSurfaceMapping.hpp"

namespace Astrea::Shell::ContextMenuSurfaceMapping {

bool overlayShouldMap(const QString &bundleOutputKey, bool bundleMapped,
                      bool controllerHasActivePresentation,
                      const QString &controllerOutputKey)
{
    return bundleMapped && controllerHasActivePresentation
        && !bundleOutputKey.isEmpty() && bundleOutputKey == controllerOutputKey;
}

} // namespace Astrea::Shell::ContextMenuSurfaceMapping
