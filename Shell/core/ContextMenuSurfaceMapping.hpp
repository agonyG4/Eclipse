#pragma once

#include <QString>

namespace Astrea::Shell::ContextMenuSurfaceMapping {

bool overlayShouldMap(const QString &bundleOutputKey, bool bundleMapped,
                      bool controllerHasActivePresentation,
                      const QString &controllerOutputKey);

} // namespace Astrea::Shell::ContextMenuSurfaceMapping
