#include "ContextMenuPlacement.hpp"

#include <algorithm>

namespace Astrea::Shell {

ContextMenuPlacement::Result ContextMenuPlacement::place(const Request &request)
{
    const QRect output = request.output.normalized();
    const int width = std::max(0, request.menuSize.width());
    const int height = std::max(0, request.menuSize.height());
    const int right = output.left() + std::max(0, output.width() - width);
    const int bottom = output.top() + std::max(0, output.height() - height);

    Result result;
    int x = request.anchor.x();
    int y = request.anchor.y();

    switch (request.kind) {
    case Kind::Point:
        if (x + width > output.right() + 1) {
            x -= width;
            result.flippedX = true;
        }
        if (y + height > output.bottom() + 1) {
            y -= height;
            result.flippedY = true;
        }
        break;
    case Kind::Rectangle:
        x = request.sourceRect.left();
        y = request.sourceRect.bottom() + 1;
        if (y + height > output.bottom() + 1) {
            y = request.sourceRect.top() - height;
            result.flippedY = true;
        }
        break;
    case Kind::Dock:
        x = request.sourceRect.center().x() - width / 2;
        y = request.sourceRect.top() - height;
        if (y < output.top()) {
            y = request.sourceRect.bottom() + 1;
            result.flippedY = true;
        }
        break;
    case Kind::Submenu:
        y = request.parentRect.top();
        if (request.direction == Qt::RightToLeft) {
            x = request.parentRect.left() - width;
            if (x < output.left()) {
                x = request.parentRect.right() + 1;
                result.flippedX = true;
            }
        } else {
            x = request.parentRect.right() + 1;
            if (x + width > output.right() + 1) {
                x = request.parentRect.left() - width;
                result.flippedX = true;
            }
        }
        break;
    }

    result.position = QPoint(clampCoordinate(x, output.left(), right),
                             clampCoordinate(y, output.top(), bottom));
    return result;
}

int ContextMenuPlacement::clampCoordinate(int value, int minimum, int maximum)
{
    return std::clamp(value, minimum, std::max(minimum, maximum));
}

} // namespace Astrea::Shell
