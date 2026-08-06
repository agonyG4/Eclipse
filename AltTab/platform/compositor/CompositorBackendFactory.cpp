#include "platform/compositor/CompositorBackendFactory.hpp"

#include "platform/compositor/AutoCompositorBackend.hpp"
#include "platform/hyprland/HyprlandWindowSource.hpp"
#include "platform/typhon/TyphonWindowSource.hpp"

CompositorBackend* CompositorBackendFactory::createBackend(const QString &requested, QObject *parent) {
    const QString target = requested.toLower();

    if (target == QStringLiteral("typhon"))
        return new TyphonWindowSource(nullptr, parent);
    if (target == QStringLiteral("hyprland"))
        return new HyprlandWindowSource(parent);
    if (target == QStringLiteral("auto"))
        return new AutoCompositorBackend({}, parent);
    return nullptr;
}
