#include "platform/runtime/AltTabRuntimePaths.hpp"
#include <QProcessEnvironment>

AltTabRuntimePaths AltTabRuntimePaths::fromEnvironment() {
    auto env = QProcessEnvironment::systemEnvironment();
    QString root = env.value(QStringLiteral("ASTREA_ROOT"));
    if (!root.isEmpty())
        return AltTabRuntimePaths(root);
    return AltTabRuntimePaths(QDir::homePath() + QStringLiteral("/.local/share/Astrea"));
}

QString AltTabRuntimePaths::alttabConfigPath() const {
    return QDir::homePath() + QStringLiteral("/.config/AstreaOS/alttab.json");
}

QString AltTabRuntimePaths::componentsConfigPath() const {
    return QDir::homePath() + QStringLiteral("/.config/AstreaOS/ui/components.json");
}
