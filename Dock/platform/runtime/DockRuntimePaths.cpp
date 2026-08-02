#include "platform/runtime/DockRuntimePaths.hpp"

#include <QDir>
#include <QProcessEnvironment>

DockRuntimePaths DockRuntimePaths::fromEnvironment()
{
    const auto environment = QProcessEnvironment::systemEnvironment();
    const QString root = environment.value(QStringLiteral("ASTREA_ROOT"));
    return DockRuntimePaths(root.isEmpty()
                                ? QDir::homePath() + QStringLiteral("/.local/share/Astrea")
                                : root);
}

QString DockRuntimePaths::dockConfigPath() const
{
    return QDir::homePath() + QStringLiteral("/.config/AstreaOS/dock.json");
}

QString DockRuntimePaths::componentsConfigPath() const
{
    return QDir::homePath() + QStringLiteral("/.config/AstreaOS/ui/components.json");
}
