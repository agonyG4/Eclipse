#pragma once

#include "services/DockConfigWatcher.hpp"

#include <QString>

class QQuickWindow;
class QScreen;

class DockLayerShellSurface final {
public:
    static bool configure(QQuickWindow *window, const DockConfig &config, int surfaceHeight,
                          QScreen *screen = nullptr, QString *errorOut = nullptr);
    static bool updateExclusiveZone(QQuickWindow *window, int surfaceHeight,
                                    QString *errorOut = nullptr);
    static bool setMapped(QQuickWindow *window, bool mapped, QString *errorOut = nullptr);
    static bool compiled();
};
