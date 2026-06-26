#pragma once

#include <QString>

class QQuickWindow;

class AltTabLayerShellSurface {
public:
    static bool configure(QQuickWindow *window, QString *errorOut = nullptr);
};
