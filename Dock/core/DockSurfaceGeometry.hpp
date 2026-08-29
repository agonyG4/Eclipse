#pragma once

#include <QObject>
#include <QRect>
#include <QSize>

class DockSurfaceGeometry final : public QObject {
    Q_OBJECT

public:
    explicit DockSurfaceGeometry(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

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
