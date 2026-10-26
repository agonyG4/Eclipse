#pragma once

#include <QString>
#include <QtCore/qglobal.h>

class QQmlEngine;

#if defined(ASTREA_SDK_CORE_BUILD)
#define ASTREA_SDK_EXPORT Q_DECL_EXPORT
#else
#define ASTREA_SDK_EXPORT Q_DECL_IMPORT
#endif

namespace AstreaSDK {

// Installs Astrea.Shared's icon image provider and its QML context property.
// The engine takes ownership of a newly installed provider. Repeated calls on
// an initialized engine are safe and leave the existing provider in place.
class ASTREA_SDK_EXPORT Core final {
public:
    [[nodiscard]] static bool initializeQmlEngine(QQmlEngine &engine,
                                                   QString *error = nullptr);
};

} // namespace AstreaSDK

#undef ASTREA_SDK_EXPORT
