#include "windowpreview/DockWindowPreviewImageProvider.hpp"

#include "windowpreview/DockWindowPreviewService.hpp"

DockWindowPreviewImageProvider::DockWindowPreviewImageProvider(
    DockWindowPreviewService *service)
    : QQuickImageProvider(QQuickImageProvider::Image), m_service(service)
{
}

QImage DockWindowPreviewImageProvider::requestImage(const QString &id, QSize *size,
                                                     const QSize &requestedSize)
{
    if (!m_service)
        return {};
    const QByteArray encodedId = id.section(QLatin1Char('?'), 0, 0).toLatin1();
    const QByteArray windowIdBytes = QByteArray::fromHex(encodedId);
    const QString windowId = QString::fromUtf8(windowIdBytes);
    const bool detail = id.section(QLatin1Char('?'), 1).split(QLatin1Char('&'))
                            .contains(QStringLiteral("detail=1"));
    const QImage original = detail ? m_service->detailImageForWindow(windowId)
                                   : m_service->imageForWindow(windowId);
    if (size)
        *size = original.size();
    if (requestedSize.isValid() && !original.isNull())
        return original.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return original;
}
