#pragma once

#include "dock/DockConfig.hpp"

#include <QObject>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QVariantMap>

struct DockSurfacePlacement final {
    QString position = QStringLiteral("bottom");
    int layerShellEdgeMargin = 0;
    int chromeEdgeInset = 0;
    bool physicalEdgeReveal = false;

    friend bool operator==(const DockSurfacePlacement &, const DockSurfacePlacement &)
        = default;
};

struct DockTooltipPlacement final {
    QRect cardRect;
    QString pointerEdge;
    int pointerCenter = 0;
};

class DockSurfaceGeometry final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int tooltipBodyHeight READ tooltipBodyHeight CONSTANT)
    Q_PROPERTY(int tooltipPointerWidth READ tooltipPointerWidth CONSTANT)
    Q_PROPERTY(int tooltipPointerDepth READ tooltipPointerDepth CONSTANT)
    Q_PROPERTY(int tooltipTargetGap READ tooltipTargetGap CONSTANT)

public:
    static constexpr int TooltipBodyHeight = 34;
    static constexpr int TooltipPointerWidth = 14;
    static constexpr int TooltipPointerDepth = 8;
    static constexpr int TooltipTargetGap = 3;

    explicit DockSurfaceGeometry(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    int tooltipBodyHeight() const { return TooltipBodyHeight; }
    int tooltipPointerWidth() const { return TooltipPointerWidth; }
    int tooltipPointerDepth() const { return TooltipPointerDepth; }
    int tooltipTargetGap() const { return TooltipTargetGap; }

    static DockSurfacePlacement placementFor(const DockConfig &config, bool autoHideActive);
    static QRect delegateRectInOutput(const QSize &outputSize, const QSize &surfaceSize,
                                      int bottomMargin, const QRectF &delegateRect);
    static QRect delegateRectInOutput(const QSize &outputSize, const QSize &surfaceSize,
                                      const QString &position, int edgeMargin,
                                      const QRectF &delegateRect);
    static QRect restingIconRectInGlobal(const QSize &outputSize, const QSize &surfaceSize,
                                         const QPoint &outputOrigin, const QString &position,
                                         int edgeMargin, int chromeEdgeInset, int iconSize,
                                         int delegateWidth, int delegateHeight, int itemSpacing,
                                         int panelPadding, int index, int count);
    static DockTooltipPlacement calculateTooltipPlacement(const QSize &outputSize,
                                                            const QRectF &anchorRect,
                                                            const QSize &cardSize,
                                                            const QString &position);
    Q_INVOKABLE QRect outputLocalDelegateRect(int outputWidth, int outputHeight,
                                               int surfaceWidth, int surfaceHeight,
                                               const QString &position, int edgeMargin,
                                               const QRectF &delegateRect) const;
    Q_INVOKABLE QVariantMap tooltipPlacementInOutput(int outputWidth, int outputHeight,
                                                      const QRectF &anchorRect, int cardWidth,
                                                      int cardHeight,
                                                      const QString &position) const;
    Q_INVOKABLE QRect restingIconRectInGlobal(int outputWidth, int outputHeight,
                                              int surfaceWidth, int surfaceHeight,
                                              int outputOriginX, int outputOriginY,
                                              const QString &position, int edgeMargin,
                                              int chromeEdgeInset, int iconSize,
                                              int delegateWidth, int delegateHeight,
                                              int itemSpacing, int panelPadding, int index,
                                              int count) const;
};
