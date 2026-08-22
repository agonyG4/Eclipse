#pragma once

#include <QByteArray>
#include <QImage>
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

struct PixmapData {
    int width = 0;
    int height = 0;
    QByteArray argb32Network;
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
Q_DECLARE_METATYPE(Astrea::StatusNotifier::ItemSnapshot)
