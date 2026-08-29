#pragma once

#include "dock/DockConfig.hpp"

#include <QObject>
#include <QRect>
#include <QSize>

struct DockSurfacePlacement final {
    int layerShellEdgeMargin = 0;
    int chromeEdgeInset = 0;
    bool physicalEdgeReveal = false;

    friend constexpr bool operator==(const DockSurfacePlacement &, const DockSurfacePlacement &)
        = default;
};

class DockSurfaceGeometry final : public QObject {
    Q_OBJECT

public:
    explicit DockSurfaceGeometry(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    static DockSurfacePlacement placementFor(const DockConfig &config, bool autoHideActive);
    static QRect delegateRectInOutput(const QSize &outputSize, const QSize &surfaceSize,
                                      int bottomMargin, const QRectF &delegateRect);
    static QRect delegateRectInOutput(const QSize &outputSize, const QSize &surfaceSize,
                                      const QString &position, int edgeMargin,
                                      const QRectF &delegateRect);
    Q_INVOKABLE QRect outputLocalDelegateRect(int outputWidth, int outputHeight,
                                               int surfaceWidth, int surfaceHeight,
                                               const QString &position, int edgeMargin,
                                               const QRectF &delegateRect) const;
};
