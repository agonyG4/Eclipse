#pragma once

#include <QString>
#include <QtCore/qglobal.h>

class QQmlEngine;

#include <AstreaSDK/Export.hpp>

namespace AstreaSDK {

class IconThemeService;

// Installs Astrea.Shared's icon image provider and its QML context property.
// The engine takes ownership of a newly installed provider. Repeated calls on
// an initialized engine are safe and leave the existing provider in place.
class ASTREA_SDK_EXPORT Core final {
public:
    // Optional file-icon URL support. service is application-owned and should
    // outlive the engine. A second call must use the same service.
    [[nodiscard]] static bool initializeFileIcons(QQmlEngine &engine,
        IconThemeService &service, QString *error = nullptr);
    [[nodiscard]] static bool initializeQmlEngine(QQmlEngine &engine,
                                                   QString *error = nullptr);
};

} // namespace AstreaSDK
