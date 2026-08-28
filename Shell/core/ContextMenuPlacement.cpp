#include "ContextMenuPlacement.hpp"

#include <algorithm>

namespace Astrea::Shell {

ContextMenuPlacement::Result ContextMenuPlacement::place(const Request &request)
{
    const QRect output = request.output.normalized();
    const int width = std::max(0, request.menuSize.width());
    const int height = std::max(0, request.menuSize.height());
    const int requestedMargin = std::max(0, request.edgeMargin);
    const int horizontalMargin = width + requestedMargin * 2 <= output.width()
        ? requestedMargin : 0;
    const int verticalMargin = height + requestedMargin * 2 <= output.height()
        ? requestedMargin : 0;
    const int left = output.left() + horizontalMargin;
    const int top = output.top() + verticalMargin;
    const int right = output.left() + std::max(0, output.width() - width - horizontalMargin);
    const int bottom = output.top() + std::max(0, output.height() - height - verticalMargin);

    Result result;
    int x = request.anchor.x();
    int y = request.anchor.y();

    switch (request.kind) {
    case Kind::Point:
        if (x > right) {
            x -= width;
            result.flippedX = true;
        }
        if (y > bottom) {
            y -= height;
            result.flippedY = true;
        }
        break;
    case Kind::Rectangle:
        x = request.sourceRect.left();
        y = request.sourceRect.bottom() + 1;
        if (y > bottom) {
            y = request.sourceRect.top() - height;
            result.flippedY = true;
        }
        break;
    case Kind::CenteredRectangle:
        x = request.sourceRect.left() + request.sourceRect.width() / 2 - width / 2;
        y = request.preferredTop;
        break;
    case Kind::Dock:
        x = request.sourceRect.center().x() - width / 2;
        y = request.sourceRect.top() - height;
        if (y < top) {
            y = request.sourceRect.bottom() + 1;
            result.flippedY = true;
        }
        break;
    case Kind::Submenu:
        y = request.parentRect.top();
        if (request.direction == Qt::RightToLeft) {
            x = request.parentRect.left() - width;
            if (x < left) {
                x = request.parentRect.right() + 1;
                result.flippedX = true;
            }
        } else {
            x = request.parentRect.right() + 1;
            if (x > right) {
                x = request.parentRect.left() - width;
                result.flippedX = true;
            }
        }
        break;
    }

    result.position = QPoint(clampCoordinate(x, left, right),
                             clampCoordinate(y, top, bottom));
    return result;
}

int ContextMenuPlacement::clampCoordinate(int value, int minimum, int maximum)
{
    return std::clamp(value, minimum, std::max(minimum, maximum));
}

} // namespace Astrea::Shell
