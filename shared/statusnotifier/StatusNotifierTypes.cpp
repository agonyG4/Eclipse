#include "statusnotifier/StatusNotifierTypes.hpp"

#include <QDBusMetaType>
namespace Astrea::StatusNotifier {

namespace {

bool isAsciiNameComponent(const QString &component, bool allowLeadingDigit)
{
    if (component.isEmpty())
        return false;
    const auto isAllowed = [](QChar ch) {
        return (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z'))
            || (ch >= QLatin1Char('a') && ch <= QLatin1Char('z'))
            || (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
            || ch == QLatin1Char('_') || ch == QLatin1Char('-');
    };
    if (!isAllowed(component.at(0)))
        return false;
    if (!allowLeadingDigit && component.at(0) >= QLatin1Char('0')
        && component.at(0) <= QLatin1Char('9'))
        return false;
    for (const QChar ch : component.mid(1)) {
        if (!isAllowed(ch))
            return false;
    }
    return true;
}

bool isUniqueName(const QString &name)
{
    if (!name.startsWith(QLatin1Char(':')) || name.toUtf8().size() > 255)
        return false;
    const QStringList components = name.mid(1).split(QLatin1Char('.'));
    if (components.size() < 2)
        return false;
    for (const QString &component : components) {
        if (component.isEmpty())
            return false;
        if (!isAsciiNameComponent(component, true))
            return false;
    }
    return true;
}

} // namespace

bool isValidDBusServiceName(const QString &name)
{
    if (name.toUtf8().size() > 255 || name.isEmpty())
        return false;
    if (name.startsWith(QLatin1Char(':')))
        return isUniqueName(name);
    const QStringList components = name.split(QLatin1Char('.'));
    if (components.size() < 2)
        return false;
    for (const QString &component : components) {
        if (!isAsciiNameComponent(component, false))
            return false;
    }
    return true;
}

bool isValidDBusObjectPath(const QString &path)
{
    if (path.isEmpty() || !path.startsWith(QLatin1Char('/')))
        return false;
    if (path == QLatin1StringView("/"))
        return true;
    const QStringList components = path.mid(1).split(QLatin1Char('/'));
    for (const QString &component : components) {
        if (component.isEmpty())
            return false;
        for (const QChar ch : component) {
            const bool valid = (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z'))
                || (ch >= QLatin1Char('a') && ch <= QLatin1Char('z'))
                || (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
                || ch == QLatin1Char('_');
            if (!valid)
                return false;
        }
    }
    return true;
}

bool ItemAddress::isValid() const
{
    return isValidDBusServiceName(service) && isValidDBusObjectPath(objectPath)
        && objectPath != QLatin1StringView("/");
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
                                  QString *errorOut)
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
            address.objectPath = QString::fromLatin1(kDefaultStatusNotifierItemObjectPath);
        } else {
            address.service = value.left(slash);
            address.objectPath = value.mid(slash);
        }
    }
    if (!isValidDBusServiceName(address.service) || !isValidDBusObjectPath(address.objectPath)
        || address.objectPath == QLatin1StringView("/")) {
        if (errorOut)
            *errorOut = QStringLiteral("malformed StatusNotifierItem registration: %1").arg(value);
        return {};
    }
    if (address.service.startsWith(QLatin1Char(':')))
        address.uniqueOwner = address.service;
    return address;
}

void registerStatusNotifierDBusMetaTypes()
{
    static const bool registered = [] {
        qDBusRegisterMetaType<StatusNotifierPixmap>();
        qDBusRegisterMetaType<StatusNotifierPixmapList>();
        qDBusRegisterMetaType<StatusNotifierToolTip>();
        qDBusRegisterMetaType<DBusMenuPropertyUpdate>();
        qDBusRegisterMetaType<QList<DBusMenuPropertyUpdate>>();
        qDBusRegisterMetaType<DBusMenuRemovedProperties>();
        qDBusRegisterMetaType<QList<DBusMenuRemovedProperties>>();
        return true;
    }();
    Q_UNUSED(registered)
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

QDBusArgument &operator<<(QDBusArgument &argument,
                          const Astrea::StatusNotifier::StatusNotifierPixmap &pixmap)
{
    argument.beginStructure();
    argument << pixmap.width << pixmap.height << pixmap.bytes;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Astrea::StatusNotifier::StatusNotifierPixmap &pixmap)
{
    argument.beginStructure();
    argument >> pixmap.width >> pixmap.height >> pixmap.bytes;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument,
                          const Astrea::StatusNotifier::StatusNotifierToolTip &tooltip)
{
    argument.beginStructure();
    argument << tooltip.iconName << tooltip.iconPixmaps << tooltip.title << tooltip.description;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Astrea::StatusNotifier::StatusNotifierToolTip &tooltip)
{
    argument.beginStructure();
    argument >> tooltip.iconName >> tooltip.iconPixmaps >> tooltip.title >> tooltip.description;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument,
                          const Astrea::StatusNotifier::DBusMenuPropertyUpdate &update)
{
    argument.beginStructure();
    argument << update.id << update.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Astrea::StatusNotifier::DBusMenuPropertyUpdate &update)
{
    argument.beginStructure();
    argument >> update.id >> update.properties;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument,
                          const Astrea::StatusNotifier::DBusMenuRemovedProperties &removed)
{
    argument.beginStructure();
    argument << removed.id << removed.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Astrea::StatusNotifier::DBusMenuRemovedProperties &removed)
{
    argument.beginStructure();
    argument >> removed.id >> removed.properties;
    argument.endStructure();
    return argument;
}
