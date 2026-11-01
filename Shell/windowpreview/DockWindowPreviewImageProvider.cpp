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
    const QImage original = m_service->imageForWindow(QString::fromUtf8(windowIdBytes));
    if (size)
        *size = original.size();
    if (requestedSize.isValid() && !original.isNull())
        return original.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return original;
}
