#pragma once

#include <QMargins>
#include <QString>

class QQuickWindow;
class QScreen;

struct AstreaLayerShellConfig {
    enum class Layer { Background, Bottom, Top, Overlay };
    enum class KeyboardInteractivity { None, Exclusive, OnDemand };

    QString scope;
    Layer layer = Layer::Top;
    KeyboardInteractivity keyboardInteractivity = KeyboardInteractivity::None;
    bool anchorTop = false;
    bool anchorBottom = false;
    bool anchorLeft = false;
    bool anchorRight = false;
    int exclusiveZone = 0;
    QMargins margins;
    QScreen *screen = nullptr;
};

class AstreaLayerShellHelper final {
public:
    static bool prepare(QString *errorOut = nullptr);
    static bool configure(QQuickWindow *window, const AstreaLayerShellConfig &config,
                          QString *errorOut = nullptr);
    static bool compiled();
};
