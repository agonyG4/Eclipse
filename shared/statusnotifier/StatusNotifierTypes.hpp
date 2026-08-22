#pragma once

#include <QByteArray>
#include <QDBusArgument>
#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <cstdint>

namespace Astrea::StatusNotifier {

enum class WatcherMode {
    Unavailable,
    External,
    Owned,
};

enum class ItemStatus {
    Passive,
    Active,
    NeedsAttention,
    Unknown,
};

struct ItemAddress {
    QString service;
    QString objectPath;
    QString uniqueOwner;

    bool isValid() const;
    QString key() const;
    bool operator==(const ItemAddress &other) const;
};

bool isValidDBusServiceName(const QString &name);
bool isValidDBusObjectPath(const QString &path);

struct PixmapData {
    int width = 0;
    int height = 0;
    QByteArray argb32Network;
};

struct StatusNotifierPixmap {
    int width = 0;
    int height = 0;
    QByteArray bytes;

    bool operator==(const StatusNotifierPixmap &other) const
    {
        return width == other.width && height == other.height && bytes == other.bytes;
    }
};

using StatusNotifierPixmapList = QList<StatusNotifierPixmap>;

struct StatusNotifierToolTip {
    QString iconName;
    StatusNotifierPixmapList iconPixmaps;
    QString title;
    QString description;
};

struct DBusMenuPropertyUpdate {
    int id = 0;
    QVariantMap properties;

    bool operator==(const DBusMenuPropertyUpdate &other) const
    {
        return id == other.id && properties == other.properties;
    }
};

struct DBusMenuRemovedProperties {
    int id = 0;
    QStringList properties;

    bool operator==(const DBusMenuRemovedProperties &other) const
    {
        return id == other.id && properties == other.properties;
    }
};

struct PixmapDecodeResult {
    QImage image;
    QString error;

    bool ok() const { return !image.isNull() && error.isEmpty(); }
};

struct ItemSnapshot {
    ItemAddress address;
    QString id;
    QString title;
    QString category;
    ItemStatus status = ItemStatus::Unknown;
    QString iconName;
    QString attentionIconName;
    QString overlayIconName;
    QString iconThemePath;
    QList<PixmapData> pixmaps;
    QList<PixmapData> attentionPixmaps;
    QList<PixmapData> overlayPixmaps;
    QString tooltipTitle;
    QString tooltipDescription;
    QString menuPath;
    bool itemIsMenu = false;
    bool ready = false;
    quint64 generation = 0;
};

struct DBusMenuLimits {
    int maxDepth = 8;
    int maxNodes = 2048;
    int maxChildren = 256;
    int maxLabelLength = 512;
    int maxIconDataBytes = 1024 * 1024;
};

PixmapDecodeResult decodeArgb32NetworkPixmap(const PixmapData &pixmap,
                                             int maxDimension = 512,
                                             qsizetype maxBytes = 4 * 1024 * 1024);

void registerStatusNotifierDBusMetaTypes();

ItemAddress normalizeRegistration(const QString &registration,
                                  const QString &senderUniqueOwner = QString(),
                                  const QString &defaultObjectPath =
                                      QStringLiteral("/StatusNotifierItem"),
                                  QString *errorOut = nullptr);

QString itemStatusName(ItemStatus status);
ItemStatus itemStatusFromString(const QString &status);

} // namespace Astrea::StatusNotifier

Q_DECLARE_METATYPE(Astrea::StatusNotifier::ItemAddress)
Q_DECLARE_METATYPE(Astrea::StatusNotifier::PixmapData)
Q_DECLARE_METATYPE(Astrea::StatusNotifier::StatusNotifierPixmap)
Q_DECLARE_METATYPE(Astrea::StatusNotifier::StatusNotifierPixmapList)
Q_DECLARE_METATYPE(Astrea::StatusNotifier::StatusNotifierToolTip)
Q_DECLARE_METATYPE(Astrea::StatusNotifier::DBusMenuPropertyUpdate)
Q_DECLARE_METATYPE(QList<Astrea::StatusNotifier::DBusMenuPropertyUpdate>)
Q_DECLARE_METATYPE(Astrea::StatusNotifier::DBusMenuRemovedProperties)
Q_DECLARE_METATYPE(QList<Astrea::StatusNotifier::DBusMenuRemovedProperties>)
Q_DECLARE_METATYPE(Astrea::StatusNotifier::ItemSnapshot)

QDBusArgument &operator<<(QDBusArgument &argument,
                          const Astrea::StatusNotifier::StatusNotifierPixmap &pixmap);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Astrea::StatusNotifier::StatusNotifierPixmap &pixmap);
QDBusArgument &operator<<(QDBusArgument &argument,
                          const Astrea::StatusNotifier::StatusNotifierToolTip &tooltip);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Astrea::StatusNotifier::StatusNotifierToolTip &tooltip);
QDBusArgument &operator<<(QDBusArgument &argument,
                          const Astrea::StatusNotifier::DBusMenuPropertyUpdate &update);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Astrea::StatusNotifier::DBusMenuPropertyUpdate &update);
QDBusArgument &operator<<(QDBusArgument &argument,
                          const Astrea::StatusNotifier::DBusMenuRemovedProperties &removed);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Astrea::StatusNotifier::DBusMenuRemovedProperties &removed);
