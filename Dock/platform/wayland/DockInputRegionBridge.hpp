#pragma once

#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QRegion>
#include <QVariantList>
#include <QVector>

class QQuickWindow;

class DockInputRegionBridge final : public QObject {
    Q_OBJECT

public:
    explicit DockInputRegionBridge(QObject *parent = nullptr);

    void setWindow(QQuickWindow *window);

    Q_INVOKABLE void update(const QRectF &chromeRect,
                            const QVariantList &interactionRects,
                            int maximumInteractionRects);

signals:
    void windowReady();
    void regionApplied();

private:
    QPointer<QQuickWindow> m_window;
    QVector<QRectF> m_interactionRects;
    QRegion m_lastRegion;
    bool m_hasAppliedRegion = false;
};
