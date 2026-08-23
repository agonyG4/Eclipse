#include "statusnotifier/StatusNotifierIconStore.hpp"

#include "icons/AstreaIconTheme.hpp"

#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QPainter>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

namespace Astrea::StatusNotifier {

StatusNotifierIconStore::StatusNotifierIconStore(QObject *parent)
    : QObject(parent)
{
    const QStringList paths = AstreaIconTheme::searchPaths();
    if (!paths.isEmpty())
        m_themeWatcher.addPaths(paths);
    connect(&m_themeWatcher, &QFileSystemWatcher::directoryChanged, this,
            &StatusNotifierIconStore::invalidateThemeIcons);
    connect(&m_themeWatcher, &QFileSystemWatcher::fileChanged, this,
            &StatusNotifierIconStore::invalidateThemeIcons);
}

QImage StatusNotifierIconStore::selectPixmap(const QList<PixmapData> &pixmaps,
                                              const QSize &requestedSize, QString *errorOut)
{
    if (pixmaps.isEmpty())
        return {};
    const int target = qMax(1, qMax(requestedSize.width(), requestedSize.height()));
    struct Candidate { QImage image; int distance = 0; bool larger = false; };
    QList<Candidate> candidates;
    for (const PixmapData &pixmap : pixmaps) {
        const auto decoded = decodeArgb32NetworkPixmap(pixmap);
        if (!decoded.ok()) {
            if (errorOut && errorOut->isEmpty())
                *errorOut = decoded.error;
            continue;
        }
        const int size = qMax(pixmap.width, pixmap.height);
        candidates.append({decoded.image, qAbs(size - target), size >= target});
    }
    if (candidates.isEmpty())
        return {};
    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate &left,
                                                               const Candidate &right) {
        if (left.larger != right.larger)
            return left.larger;
        if (left.distance != right.distance)
            return left.distance < right.distance;
        return left.image.width() * left.image.height()
            < right.image.width() * right.image.height();
    });
    return candidates.constFirst().image;
}

QImage StatusNotifierIconStore::loadNamedIcon(const QString &name, const QString &itemThemePath,
                                               const QSize &requestedSize)
{
    if (name.isEmpty())
        return {};
    const int target = qMax(1, qMax(requestedSize.width(), requestedSize.height()));
    if (QFileInfo::exists(name)) {
        QImage image(name);
        if (!image.isNull())
            return image;
    }
    if (!itemThemePath.isEmpty()) {
        const QStringList subdirs{QStringLiteral("scalable"), QStringLiteral("symbolic"),
                                  QStringLiteral("%1x%1").arg(target),
                                  QStringLiteral("16x16"), QStringLiteral("22x22"),
                                  QStringLiteral("24x24"), QStringLiteral("32x32"),
                                  QStringLiteral("48x48"), QStringLiteral("64x64")};
        const QStringList extensions{QStringLiteral(""), QStringLiteral(".png"),
                                     QStringLiteral(".svg"), QStringLiteral(".svgz")};
        for (const QString &subdir : subdirs) {
            for (const QString &extension : extensions) {
                const QString candidate = QDir(itemThemePath).filePath(
                    subdir + QLatin1Char('/') + name + extension);
                if (QFileInfo::exists(candidate)) {
                    QImage image(candidate);
                    if (!image.isNull())
                        return image;
                }
            }
        }
    }
    const QIcon icon = QIcon::fromTheme(name);
    if (!icon.isNull())
        return icon.pixmap(target, target).toImage();
    return {};
}

QImage StatusNotifierIconStore::compose(const QImage &base, const QImage &overlay)
{
    if (base.isNull())
        return overlay;
    if (overlay.isNull())
        return base;
    QImage result = base.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&result);
    const QSize size = overlay.size().scaled(result.size(), Qt::KeepAspectRatio);
    painter.drawImage(QRect(result.width() - size.width(), result.height() - size.height(),
                            size.width(), size.height()), overlay);
    return result;
}

QImage StatusNotifierIconStore::fallbackImage(const Entry &entry, const QSize &requestedSize)
{
    QString ignored;
    QImage image = entry.named;
    if (image.isNull())
        image = selectPixmap(entry.snapshot.pixmaps, requestedSize, &ignored);
    if (entry.snapshot.status == ItemStatus::NeedsAttention) {
        QImage attention = entry.attention;
        if (attention.isNull())
            attention = selectPixmap(entry.snapshot.attentionPixmaps, requestedSize, &ignored);
        if (!attention.isNull())
            image = attention;
    }
    QImage overlay = entry.overlay;
    if (overlay.isNull())
        overlay = selectPixmap(entry.snapshot.overlayPixmaps, requestedSize, &ignored);
    return compose(image, overlay);
}

void StatusNotifierIconStore::updateItem(const ItemSnapshot &snapshot)
{
    if (!snapshot.address.isValid())
        return;
    Entry &entry = m_entries[snapshot.address.key()];
    entry.snapshot = snapshot;
    entry.named = loadNamedIcon(snapshot.iconName, snapshot.iconThemePath, QSize(32, 32));
    entry.attention = loadNamedIcon(snapshot.attentionIconName, snapshot.iconThemePath,
                                    QSize(32, 32));
    entry.overlay = loadNamedIcon(snapshot.overlayIconName, snapshot.iconThemePath,
                                  QSize(16, 16));
    entry.revision = m_nextRevision++;
    emit itemIconChanged(snapshot.address.key(), entry.revision);
}

void StatusNotifierIconStore::updateAuxiliaryImage(const QString &key, const QImage &image)
{
    if (key.isEmpty())
        return;
    if (image.isNull()) {
        if (m_auxiliaryImages.remove(key) > 0) {
            m_auxiliaryRevisions.insert(key, m_nextRevision);
            emit itemIconChanged(key, m_nextRevision++);
        }
        return;
    }
    m_auxiliaryImages.insert(key, image);
    m_auxiliaryRevisions.insert(key, m_nextRevision);
    emit itemIconChanged(key, m_nextRevision++);
}

void StatusNotifierIconStore::updateAuxiliaryNamedImage(const QString &key, const QString &name,
                                                        const QString &themePath)
{
    updateAuxiliaryImage(key, loadNamedIcon(name, themePath, QSize(32, 32)));
}

void StatusNotifierIconStore::clearAuxiliaryImages(const QString &prefix)
{
    const QStringList keys = m_auxiliaryImages.keys();
    for (const QString &key : keys) {
        if (!key.startsWith(prefix))
            continue;
        m_auxiliaryImages.remove(key);
        m_auxiliaryRevisions.remove(key);
        emit itemIconChanged(key, m_nextRevision++);
    }
}

void StatusNotifierIconStore::clearItem(const QString &itemKey)
{
    clearAuxiliaryImages(QStringLiteral("menu:%1:").arg(itemKey));
    if (m_entries.remove(itemKey) > 0)
        emit itemIconChanged(itemKey, m_nextRevision++);
}

void StatusNotifierIconStore::clear()
{
    const auto keys = m_entries.keys();
    m_entries.clear();
    for (const QString &key : keys)
        emit itemIconChanged(key, m_nextRevision++);
    const auto auxiliaryKeys = m_auxiliaryImages.keys();
    m_auxiliaryImages.clear();
    m_auxiliaryRevisions.clear();
    for (const QString &key : auxiliaryKeys)
        emit itemIconChanged(key, m_nextRevision++);
}

QImage StatusNotifierIconStore::image(const QString &itemKey, const QSize &requestedSize) const
{
    const auto auxiliary = m_auxiliaryImages.constFind(itemKey);
    if (auxiliary != m_auxiliaryImages.constEnd())
        return auxiliary.value().scaled(requestedSize, Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation);
    const auto it = m_entries.constFind(itemKey);
    if (it == m_entries.constEnd())
        return {};
    return fallbackImage(it.value(), requestedSize);
}

QPixmap StatusNotifierIconStore::pixmap(const QString &itemKey, const QSize &requestedSize) const
{
    return QPixmap::fromImage(image(itemKey, requestedSize));
}

QString StatusNotifierIconStore::imageSource(const QString &itemKey) const
{
    if (!m_auxiliaryImages.contains(itemKey) && !m_entries.contains(itemKey))
        return {};
    return QStringLiteral("image://astrea-tray/%1?revision=%2")
        .arg(QString::fromUtf8(QUrl::toPercentEncoding(itemKey)))
        .arg(revision(itemKey));
}

quint64 StatusNotifierIconStore::revision(const QString &itemKey) const
{
    if (m_auxiliaryImages.contains(itemKey))
        return m_auxiliaryRevisions.value(itemKey);
    return m_entries.value(itemKey).revision;
}

bool StatusNotifierIconStore::hasIcon(const QString &itemKey) const
{
    return m_auxiliaryImages.contains(itemKey) || !image(itemKey).isNull();
}

void StatusNotifierIconStore::invalidateThemeIcons()
{
    AstreaIconTheme::apply();
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        it->named = loadNamedIcon(it->snapshot.iconName, it->snapshot.iconThemePath, QSize(32, 32));
        it->attention = loadNamedIcon(it->snapshot.attentionIconName, it->snapshot.iconThemePath,
                                      QSize(32, 32));
        it->overlay = loadNamedIcon(it->snapshot.overlayIconName, it->snapshot.iconThemePath,
                                    QSize(16, 16));
        it->revision = m_nextRevision++;
        emit itemIconChanged(it.key(), it->revision);
    }
    emit themeIconsInvalidated();
}

} // namespace Astrea::StatusNotifier
