#pragma once

#include "services/DockConfigWatcher.hpp"
#include "platform/wayland/LayerShellHelper.hpp"

#include <QString>
#include <QtGlobal>

class QQuickWindow;
class QScreen;

class DockLayerShellSurface final {
public:
    static AstreaLayerShellConfig configurationFor(const DockConfig &config,
                                                   int exclusiveZoneHeight,
                                                   QScreen *screen = nullptr);
    static bool configure(QQuickWindow *window, const DockConfig &config,
                          int exclusiveZoneHeight,
                          QScreen *screen = nullptr, QString *errorOut = nullptr);
    // Publish the compositor's logical output geometry to the Dock root. The
    // Dock window itself remains content-sized; its context-menu anchor math
    // consumes these output-local dimensions.
    static bool setOutputGeometry(QQuickWindow *window, QScreen *screen,
                                  QString *errorOut = nullptr);
    static bool updateExclusiveZone(QQuickWindow *window, int exclusiveZoneHeight,
                                    QString *errorOut = nullptr);
    static bool setMapped(QQuickWindow *window, bool mapped, int exclusiveZoneHeight,
                          QString *errorOut = nullptr);
    static int exclusiveZoneForMapping(bool mapped, int restingHeight)
    { return mapped ? qMax(0, restingHeight) : 0; }
    static bool compiled();
};
