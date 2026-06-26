#include "platform/compositor/CompositorBackendFactory.hpp"
#include "platform/hyprland/HyprlandWindowSource.hpp"
#include <QProcessEnvironment>
#include <QDir>
#include <QDebug>

CompositorBackend* CompositorBackendFactory::createBackend(const QString &requested, QObject *parent) {
    const QString target = requested.toLower();
    const auto env = QProcessEnvironment::systemEnvironment();

    if (target == QStringLiteral("hyprland")
        || (target == QStringLiteral("auto") && !env.value(QStringLiteral("HYPRLAND_INSTANCE_SIGNATURE")).isEmpty())) {
        return new HyprlandWindowSource(parent);
    }

    qWarning("No supported compositor backend available (requested: %s)", qPrintable(requested));
    return nullptr;
}
