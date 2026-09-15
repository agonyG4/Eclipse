#include "screenshot/ScreenshotInputRegionBridge.hpp"

#include <QQuickWindow>

ScreenshotInputRegionBridge::ScreenshotInputRegionBridge(QObject *parent)
    : QObject(parent)
{
}

void ScreenshotInputRegionBridge::setWindow(QQuickWindow *window)
{
    m_window = window;
    m_region = {};
}

void ScreenshotInputRegionBridge::update(const QRectF &rect)
{
    if (!m_window)
        return;
    QRegion next = QRegion(rect.toAlignedRect()).intersected(QRect(QPoint(0, 0), m_window->size()));
    if (next.isEmpty() && !m_window->size().isEmpty())
        next = QRegion(QRect(0, 0, 1, 1));
    if (next == m_region)
        return;
    m_region = next;
    m_window->setMask(next);
    emit regionApplied();
}

void ScreenshotInputRegionBridge::suspend()
{
    if (!m_window)
        return;

    QRegion next;
    if (!m_window->size().isEmpty())
        next = QRegion(QRect(0, 0, 1, 1));
    if (next == m_region)
        return;
    m_region = next;
    m_window->setMask(next);
    emit regionApplied();
}
