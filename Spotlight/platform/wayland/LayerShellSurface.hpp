#pragma once

#include <QString>

class QQuickWindow;

class LayerShellSurface {
public:
    static QString prepareEarly();
    static bool configure(QQuickWindow *window, QString *errorOut = nullptr);
};
