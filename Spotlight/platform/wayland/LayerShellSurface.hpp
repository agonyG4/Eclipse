#pragma once

#include <QString>

class QQuickWindow;

class LayerShellSurface {
public:
    static bool configure(QQuickWindow *window, QString *errorOut = nullptr);
};
