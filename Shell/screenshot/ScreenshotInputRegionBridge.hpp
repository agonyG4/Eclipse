#pragma once

#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QRegion>

class QQuickWindow;

class ScreenshotInputRegionBridge final : public QObject {
    Q_OBJECT

public:
    explicit ScreenshotInputRegionBridge(QObject *parent = nullptr);

    void setWindow(QQuickWindow *window);
    QRegion region() const { return m_region; }

    Q_INVOKABLE void update(const QRectF &rect);
    Q_INVOKABLE void suspend();

signals:
    void regionApplied();

private:
    QPointer<QQuickWindow> m_window;
    QRegion m_region;
};
