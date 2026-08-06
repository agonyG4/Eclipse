#include "platform/compositor/CompositorBackendFactory.hpp"
#include "platform/hyprland/HyprlandWindowSource.hpp"
#include "platform/typhon/TyphonWindowSource.hpp"
#include <QProcessEnvironment>
#include <QDebug>

CompositorBackend* CompositorBackendFactory::createBackend(const QString &requested, QObject *parent) {
    const QString target = requested.toLower();
    const auto env = QProcessEnvironment::systemEnvironment();

    if (target == QStringLiteral("typhon")
        || (target == QStringLiteral("auto")
            && TyphonWindowSource::protocolAvailableOnCurrentDisplay())) {
        return new TyphonWindowSource(nullptr, parent);
    }

    if (target == QStringLiteral("hyprland")
        || (target == QStringLiteral("auto") && !env.value(QStringLiteral("HYPRLAND_INSTANCE_SIGNATURE")).isEmpty())) {
        return new HyprlandWindowSource(parent);
    }

    qWarning("No supported compositor backend available (requested: %s)", qPrintable(requested));
    return nullptr;
}
