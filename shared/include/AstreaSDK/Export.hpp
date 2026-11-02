#pragma once
#include <QtCore/qglobal.h>
#if defined(ASTREA_SDK_CORE_BUILD)
#define ASTREA_SDK_EXPORT Q_DECL_EXPORT
#else
#define ASTREA_SDK_EXPORT Q_DECL_IMPORT
#endif
