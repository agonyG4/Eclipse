#pragma once

#include <QPointF>
#include <QRectF>

#include <optional>

struct AstreaAnalyticRoundedRect final {
    QRectF rect;
    qreal radius = 0.0;

    friend bool operator==(const AstreaAnalyticRoundedRect &,
                           const AstreaAnalyticRoundedRect &) = default;
};

struct AstreaAnalyticTriangle final {
    QPointF a;
    QPointF b;
    QPointF c;

    friend bool operator==(const AstreaAnalyticTriangle &,
                           const AstreaAnalyticTriangle &) = default;
};

// Resolved shape geometry in window-content / wl_surface-local logical pixels.
// It intentionally contains no Qt item or Wayland proxy references.
struct AstreaAnalyticBackdropCoverage final {
    std::optional<AstreaAnalyticRoundedRect> roundedRect;
    std::optional<AstreaAnalyticTriangle> triangle;

    bool isEmpty() const { return !roundedRect && !triangle; }

    friend bool operator==(const AstreaAnalyticBackdropCoverage &,
                           const AstreaAnalyticBackdropCoverage &) = default;
};
