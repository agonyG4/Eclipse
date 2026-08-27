#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>

namespace Astrea::Shell {

class ContextMenuPlacement final {
public:
    enum class Kind {
        Point,
        Rectangle,
        Dock,
        Submenu,
    };

    struct Request {
        QRect output;
        QSize menuSize;
        QPoint anchor;
        QRect sourceRect;
        QRect parentRect;
        Kind kind = Kind::Point;
        Qt::LayoutDirection direction = Qt::LeftToRight;
    };

    struct Result {
        QPoint position;
        bool flippedX = false;
        bool flippedY = false;
    };

    static Result place(const Request &request);

private:
    static int clampCoordinate(int value, int minimum, int maximum);
};

} // namespace Astrea::Shell
