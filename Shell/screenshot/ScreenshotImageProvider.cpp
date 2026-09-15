#include "screenshot/ScreenshotImageProvider.hpp"

#include "screenshot/ScreenshotController.hpp"

ScreenshotImageProvider::ScreenshotImageProvider(ScreenshotController *controller)
    : QQuickImageProvider(QQuickImageProvider::Image), m_controller(controller)
{
}

QImage ScreenshotImageProvider::requestImage(const QString &, QSize *size,
                                             const QSize &requestedSize)
{
    if (!m_controller)
        return {};
    QImage image = m_controller->image();
    if (size)
        *size = image.size();
    if (requestedSize.isValid() && !image.isNull())
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image;
}
