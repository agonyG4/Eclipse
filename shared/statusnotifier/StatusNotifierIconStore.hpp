#pragma once

#include "statusnotifier/StatusNotifierTypes.hpp"

#include <QObject>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QFileSystemWatcher>

namespace Astrea::StatusNotifier {

class StatusNotifierIconStore final : public QObject {
    Q_OBJECT

public:
    explicit StatusNotifierIconStore(QObject *parent = nullptr);

    void updateItem(const ItemSnapshot &snapshot);
    void updateAuxiliaryImage(const QString &key, const QImage &image);
    void updateAuxiliaryNamedImage(const QString &key, const QString &name,
                                   const QString &themePath = QString());
    void clearAuxiliaryImages(const QString &prefix);
    void clearItem(const QString &itemKey);
    void clear();

    QImage image(const QString &itemKey, const QSize &requestedSize = QSize(16, 16)) const;
    QPixmap pixmap(const QString &itemKey, const QSize &requestedSize = QSize(16, 16)) const;
    QString imageSource(const QString &itemKey) const;
    quint64 revision(const QString &itemKey) const;
    bool hasIcon(const QString &itemKey) const;

signals:
    void itemIconChanged(const QString &itemKey, quint64 revision);
    void themeIconsInvalidated();

public slots:
    void invalidateThemeIcons();

private:
    struct Entry {
        ItemSnapshot snapshot;
        QImage named;
        QImage attention;
        QImage overlay;
        quint64 revision = 0;
    };

    static QImage selectPixmap(const QList<PixmapData> &pixmaps, const QSize &requestedSize,
                               QString *errorOut = nullptr);
    static QImage loadNamedIcon(const QString &name, const QString &itemThemePath,
                                const QSize &requestedSize);
    static QImage compose(const QImage &base, const QImage &overlay);
    static QImage fallbackImage(const Entry &entry, const QSize &requestedSize);

    QHash<QString, Entry> m_entries;
    QHash<QString, QImage> m_auxiliaryImages;
    QHash<QString, quint64> m_auxiliaryRevisions;
    QFileSystemWatcher m_themeWatcher;
    quint64 m_nextRevision = 1;
};

} // namespace Astrea::StatusNotifier
