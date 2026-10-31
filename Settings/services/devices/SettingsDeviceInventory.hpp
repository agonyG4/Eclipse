#pragma once

#include <QString>
#include <QVariantList>

// Read-only snapshot of Linux device sysfs. Never opens raw devices or mounts media.
class SettingsDeviceInventory final {
public:
    static QVariantList enumerate(const QString &sysRoot = QStringLiteral("/sys"));
};
