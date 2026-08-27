#pragma once

#include "services/DockConfigWatcher.hpp"

#include <QString>
#include <QtGlobal>

class QQuickWindow;
class QScreen;

class DockLayerShellSurface final {
public:
    static bool configure(QQuickWindow *window, const DockConfig &config,
                          int exclusiveZoneHeight,
                          QScreen *screen = nullptr, QString *errorOut = nullptr);
    static bool updateExclusiveZone(QQuickWindow *window, int exclusiveZoneHeight,
                                    QString *errorOut = nullptr);
    static bool setMapped(QQuickWindow *window, bool mapped, int exclusiveZoneHeight,
                          QString *errorOut = nullptr);
    static int exclusiveZoneForMapping(bool mapped, int restingHeight)
    { return mapped ? qMax(0, restingHeight) : 0; }
    static bool compiled();
};
