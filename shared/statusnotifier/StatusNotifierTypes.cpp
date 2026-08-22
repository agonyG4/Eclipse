#include "statusnotifier/StatusNotifierTypes.hpp"

#include <QRegularExpression>

namespace Astrea::StatusNotifier {

bool ItemAddress::isValid() const
{
    return !service.isEmpty() && service.contains(QLatin1Char('.'))
        && objectPath.startsWith(QLatin1Char('/')) && objectPath.size() > 1;
}

QString ItemAddress::key() const
{
    return service + QLatin1Char('|') + objectPath;
}

bool ItemAddress::operator==(const ItemAddress &other) const
{
    return service == other.service && objectPath == other.objectPath
        && uniqueOwner == other.uniqueOwner;
}

PixmapDecodeResult decodeArgb32NetworkPixmap(const PixmapData &pixmap, int maxDimension,
                                             qsizetype maxBytes)
{
    PixmapDecodeResult result;
    if (pixmap.width <= 0 || pixmap.height <= 0) {
        result.error = QStringLiteral("pixmap dimensions must be positive");
        return result;
    }
    if (maxDimension <= 0 || pixmap.width > maxDimension || pixmap.height > maxDimension) {
        result.error = QStringLiteral("pixmap dimensions exceed the safety limit");
        return result;
    }
    const qint64 pixelCount = qint64(pixmap.width) * qint64(pixmap.height);
    const qint64 expectedBytes = pixelCount * 4;
    if (pixelCount <= 0 || expectedBytes > maxBytes || expectedBytes > qsizetype(INT_MAX)) {
        result.error = QStringLiteral("pixmap payload exceeds the safety limit");
        return result;
    }
    if (pixmap.argb32Network.size() != expectedBytes) {
        result.error = QStringLiteral("pixmap payload length does not match dimensions");
        return result;
    }

    result.image = QImage(pixmap.width, pixmap.height, QImage::Format_ARGB32);
    if (result.image.isNull()) {
        result.error = QStringLiteral("unable to allocate pixmap image");
        return result;
    }
    for (int y = 0; y < pixmap.height; ++y) {
        auto *line = reinterpret_cast<QRgb *>(result.image.scanLine(y));
        for (int x = 0; x < pixmap.width; ++x) {
            const qsizetype offset = (qint64(y) * pixmap.width + x) * 4;
            const auto *bytes = reinterpret_cast<const uchar *>(pixmap.argb32Network.constData()
                                                                 + offset);
            // StatusNotifierItem transports ARGB32 pixels in network byte order.
            line[x] = qRgba(bytes[1], bytes[2], bytes[3], bytes[0]);
        }
    }
    return result;
}

ItemAddress normalizeRegistration(const QString &registration, const QString &senderUniqueOwner,
                                  const QString &defaultObjectPath, QString *errorOut)
{
    ItemAddress address;
    const QString value = registration.trimmed();
    const QString sender = senderUniqueOwner.trimmed();
    if (value.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("empty StatusNotifierItem registration");
        return address;
    }

    if (value.startsWith(QLatin1Char('/'))) {
        address.service = sender;
        address.objectPath = value;
    } else {
        const int slash = value.indexOf(QLatin1Char('/'));
        if (slash < 0) {
            address.service = value;
            address.objectPath = defaultObjectPath;
        } else {
            address.service = value.left(slash);
            address.objectPath = value.mid(slash);
        }
    }
    if (address.service.isEmpty() || !address.service.contains(QLatin1Char('.'))
        || !address.objectPath.startsWith(QLatin1Char('/')) || address.objectPath.size() <= 1
        || address.objectPath.contains(QRegularExpression(QStringLiteral("[^A-Za-z0-9_/]")))) {
        if (errorOut)
            *errorOut = QStringLiteral("malformed StatusNotifierItem registration: %1").arg(value);
        return {};
    }
    if (address.service.startsWith(QLatin1Char(':')))
        address.uniqueOwner = address.service;
    return address;
}

QString itemStatusName(ItemStatus status)
{
    switch (status) {
    case ItemStatus::Passive: return QStringLiteral("Passive");
    case ItemStatus::Active: return QStringLiteral("Active");
    case ItemStatus::NeedsAttention: return QStringLiteral("NeedsAttention");
    case ItemStatus::Unknown: return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

ItemStatus itemStatusFromString(const QString &status)
{
    const QString normalized = status.trimmed().toLower();
    if (normalized == QStringLiteral("passive"))
        return ItemStatus::Passive;
    if (normalized == QStringLiteral("active"))
        return ItemStatus::Active;
    if (normalized == QStringLiteral("needsattention"))
        return ItemStatus::NeedsAttention;
    return ItemStatus::Unknown;
}

} // namespace Astrea::StatusNotifier
