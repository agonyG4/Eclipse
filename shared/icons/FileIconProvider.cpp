#include <AstreaSDK/FileIconProvider.hpp>

#include <QFileInfo>
#include <QImageReader>
#include <QThread>
#include <QUrl>
#include <QUrlQuery>

#include <AstreaSDK/IconThemeService.hpp>

namespace AstreaSDK {

FileIconProvider::FileIconProvider(IconThemeService *service)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_service(service)
{
}

QImage FileIconProvider::requestImage(
    const QString &id,
    QSize *size,
    const QSize &requestedSize)
{
    if (!m_service) {
        if (size) {
            *size = requestedSize.isValid() ? requestedSize : QSize(32, 32);
        }
        return {};
    }

    // Theme icons are intentionally requested synchronously by QML. This keeps
    // QIcon/QPixmap on the GUI thread; thumbnail providers remain asynchronous.
    Q_ASSERT(QThread::currentThread() == m_service->thread());

    QString payload = id;
    QString queryText;
    const int queryIndex = payload.indexOf(QLatin1Char('?'));
    if (queryIndex >= 0) {
        queryText = payload.mid(queryIndex + 1);
        payload.truncate(queryIndex);
    }
    const QUrlQuery query(queryText);
    bool dprOk = false;
    qreal devicePixelRatio = query.queryItemValue(QStringLiteral("dpr")).toDouble(&dprOk);
    if (!dprOk || devicePixelRatio <= 0.0) {
        devicePixelRatio = 1.0;
    }
    devicePixelRatio = qBound<qreal>(0.5, devicePixelRatio, 4.0);
    const int querySize = query.queryItemValue(QStringLiteral("size")).toInt();
    const QSize logicalSize = querySize > 0
        ? QSize(querySize, querySize)
        : requestedSize.isValid()
        ? QSize(
            qMax(1, qRound(requestedSize.width() / devicePixelRatio)),
            qMax(1, qRound(requestedSize.height() / devicePixelRatio)))
        : QSize(32, 32);
    const QSize physicalSize = requestedSize.isValid()
        ? requestedSize
        : QSize(
            qMax(1, qRound(logicalSize.width() * devicePixelRatio)),
            qMax(1, qRound(logicalSize.height() * devicePixelRatio)));

    if (payload.startsWith(QStringLiteral("file/"))) {
        payload.remove(0, 5);
        const QUrl iconUrl(QUrl::fromPercentEncoding(payload.toUtf8()));
        if (iconUrl.isValid() && iconUrl.isLocalFile()) {
            const QFileInfo iconFile(iconUrl.toLocalFile());
            if (iconFile.isFile()) {
                QImageReader reader(iconFile.absoluteFilePath());
                reader.setAutoTransform(true);
                const QSize sourceSize = reader.size();
                if (sourceSize.isValid() && physicalSize.isValid()) {
                    reader.setScaledSize(sourceSize.scaled(physicalSize, Qt::KeepAspectRatio));
                }
                QImage image = reader.read();
                if (!image.isNull()) {
                    image.setDevicePixelRatio(devicePixelRatio);
                    if (size) {
                        *size = image.size();
                    }
                    return image;
                }
            }
        }

        const QString fallback = query.queryItemValue(QStringLiteral("fallback"));
        const QStringList candidates = fallback.isEmpty()
            ? QStringList{}
            : fallback.split(QLatin1Char('|'), Qt::SkipEmptyParts);
        const QImage image = m_service->renderIcon(candidates, logicalSize, devicePixelRatio);
        if (size) {
            *size = image.size();
        }
        return image;
    }
    if (payload.startsWith(QStringLiteral("theme/"))) {
        payload.remove(0, 6);
    }

    const QString decoded = QUrl::fromPercentEncoding(payload.toUtf8());
    const QStringList candidates = decoded.isEmpty()
        ? QStringList{}
        : decoded.split(QLatin1Char('|'), Qt::SkipEmptyParts);
   const QSize effectiveSize = logicalSize.isValid() ? logicalSize : QSize(32, 32);
    const QImage image = m_service->renderIcon(candidates, effectiveSize, devicePixelRatio);
    if (size) {
        *size = image.size();
    }
    return image;
}

} // namespace AstreaSDK
