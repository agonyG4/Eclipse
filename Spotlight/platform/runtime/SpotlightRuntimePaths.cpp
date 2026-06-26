#include "platform/runtime/SpotlightRuntimePaths.hpp"
#include <QStandardPaths>
#include <QProcessEnvironment>

SpotlightRuntimePaths SpotlightRuntimePaths::fromEnvironment() {
    auto env = QProcessEnvironment::systemEnvironment();
    QString root = env.value(QStringLiteral("ASTREA_ROOT"));
    if (!root.isEmpty())
        return SpotlightRuntimePaths(root);
    return SpotlightRuntimePaths(QDir::homePath() + QStringLiteral("/.local/share/Astrea"));
}

QString SpotlightRuntimePaths::configPath() const {
    return QDir::homePath() + QStringLiteral("/.config/AstreaOS/spotlight.json");
}

QString SpotlightRuntimePaths::componentsConfigPath() const {
    return QDir::homePath() + QStringLiteral("/.config/AstreaOS/ui/components.json");
}

QString SpotlightRuntimePaths::i18nDir() const {
    return m_astreaRoot + QStringLiteral("/System/i18n");
}

QString SpotlightRuntimePaths::weatherAssetDir() const {
    return m_astreaRoot + QStringLiteral("/Apps/Weather/assets/icons/weather");
}
