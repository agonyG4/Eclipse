#include "windowpreview/DockWindowPreviewService.hpp"

#include "platform/typhon/TyphonScreenCaptureClient.hpp"

#include <QImageReader>
#include <QMutexLocker>
#include <QPainter>
#include <QSet>
#include <QUrl>

#include <algorithm>

namespace {

constexpr qsizetype kMaximumCacheBytes = 64 * 1024 * 1024;
constexpr std::uint32_t kCaptureWidth = 512;
constexpr std::uint32_t kCaptureHeight = 320;
constexpr int kBackdropWidth = 320;
constexpr int kBackdropBlurWidth = 20;

} // namespace

DockWindowPreviewService::DockWindowPreviewService(TyphonScreenCaptureClient *client,
                                                   QObject *parent)
    : QObject(parent), m_client(client),
      m_currentGeneration(client ? client->connectionGeneration() : 0)
{
    if (!m_client)
        return;
    connect(m_client, &TyphonScreenCaptureClient::ready, this,
            [this](const QImage &image, quint64 generation, quint64 token) {
                finishRequest({}, image, generation, token);
            });
    connect(m_client, &TyphonScreenCaptureClient::failed, this,
            [this](const QString &reason, quint64 generation, quint64 token) {
                finishRequest(reason, {}, generation, token);
            });
    connect(m_client, &TyphonScreenCaptureClient::stateChanged, this,
            [this](const QString &state) {
                const quint64 generation = m_client->connectionGeneration();
                const bool didGenerationChange = generation != m_currentGeneration;
                if (didGenerationChange) {
                    QMutexLocker lock(&m_mutex);
                    m_cache.clear();
                    m_cachedBytes = 0;
                    m_currentGeneration = generation;
                    m_requestInFlight = false;
                    m_activeWindowId.clear();
                    m_queue.clear();
                }
                if (didGenerationChange) {
                    emit generationChanged(generation);
                }
                if (state == QStringLiteral("ready")) {
                    QStringList demanded;
                    {
                        QMutexLocker lock(&m_mutex);
                        demanded = m_minimizedWindowIds;
                        for (const QString &id : m_popupWindowIds) {
                            if (!demanded.contains(id))
                                demanded.append(id);
                        }
                    }
                    for (const QString &id : demanded)
                        requestWindowPreview(id, false);
                    requestNext();
                }
            });
}

void DockWindowPreviewService::requestWindowPreview(const QString &windowId, bool refresh)
{
    if (windowId.isEmpty())
        return;
    {
        QMutexLocker lock(&m_mutex);
        if (m_hasLiveWindowSet && !m_liveWindowIds.contains(windowId))
            return;
        const auto cached = m_cache.constFind(windowId);
        if (!refresh && cached != m_cache.constEnd()
            && cached->generation == m_currentGeneration)
            return;
        if ((m_requestInFlight && m_activeWindowId == windowId) || m_queue.contains(windowId))
            return;
        m_queue.enqueue(windowId);
    }
    requestNext();
}

QString DockWindowPreviewService::imageUrl(const QString &windowId) const
{
    QMutexLocker lock(&m_mutex);
    const auto it = m_cache.constFind(windowId);
    // No URL until a capture exists: QML shows the app icon instead of asking
    // the provider for an image it does not have.
    if (it == m_cache.constEnd() || it->generation != m_currentGeneration)
        return {};
    const quint64 revision = it->revision;
    const QString encoded = QString::fromLatin1(windowId.toUtf8().toHex());
    return QStringLiteral("image://astrea-window-preview/%1?g=%2&r=%3")
        .arg(encoded).arg(m_currentGeneration).arg(revision);
}

void DockWindowPreviewService::reconcileLiveWindowIds(const QStringList &windowIds)
{
    const QSet<QString> live(windowIds.cbegin(), windowIds.cend());
    bool cancelActive = false;
    {
        QMutexLocker lock(&m_mutex);
        m_hasLiveWindowSet = true;
        m_liveWindowIds = live.values();
        for (auto it = m_cache.begin(); it != m_cache.end();) {
            if (!live.contains(it.key())) {
                m_cachedBytes -= it->bytes;
                it = m_cache.erase(it);
            } else {
                ++it;
            }
        }
        QQueue<QString> retained;
        while (!m_queue.isEmpty()) {
            const QString candidate = m_queue.dequeue();
            if (live.contains(candidate))
                retained.enqueue(candidate);
        }
        m_queue = std::move(retained);
        if (m_requestInFlight && !live.contains(m_activeWindowId)) {
            m_requestInFlight = false;
            m_activeWindowId.clear();
            cancelActive = true;
        }
    }
    if (cancelActive && m_client)
        m_client->cancelCapture();
    requestNext();
}

void DockWindowPreviewService::setMinimizedWindowIds(const QStringList &windowIds)
{
    QStringList newlyMinimized;
    {
        QMutexLocker lock(&m_mutex);
        for (const QString &id : windowIds) {
            if (!m_minimizedWindowIds.contains(id) && !newlyMinimized.contains(id))
                newlyMinimized.append(id);
        }
        m_minimizedWindowIds = QSet<QString>(windowIds.cbegin(), windowIds.cend()).values();
    }
    cancelRequestsWithoutVisibleDemand();
    // A restored window may have changed since its previous minimized image.
    for (const QString &id : newlyMinimized)
        requestWindowPreview(id, true);
}

void DockWindowPreviewService::setPopupWindowIds(const QStringList &windowIds)
{
    {
        QMutexLocker lock(&m_mutex);
        m_popupWindowIds = QSet<QString>(windowIds.cbegin(), windowIds.cend()).values();
    }
    cancelRequestsWithoutVisibleDemand();
}

void DockWindowPreviewService::cancelRequestsWithoutVisibleDemand()
{
    bool cancelActive = false;
    {
        QMutexLocker lock(&m_mutex);
        QQueue<QString> retained;
        while (!m_queue.isEmpty()) {
            const QString candidate = m_queue.dequeue();
            if (m_minimizedWindowIds.contains(candidate) || m_popupWindowIds.contains(candidate))
                retained.enqueue(candidate);
        }
        m_queue = std::move(retained);
        if (m_requestInFlight && !m_minimizedWindowIds.contains(m_activeWindowId)
            && !m_popupWindowIds.contains(m_activeWindowId)) {
            m_requestInFlight = false;
            m_activeWindowId.clear();
            cancelActive = true;
        }
    }
    if (cancelActive && m_client)
        m_client->cancelCapture();
    requestNext();
}

void DockWindowPreviewService::setBackdropSource(const QString &source)
{
    const QUrl url(source);
    const QString path = url.isLocalFile() ? url.toLocalFile() : source;
    if (path == m_backdropSource)
        return;
    m_backdropSource = path;
    m_backdrop = {};
    if (path.isEmpty())
        return;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize sourceSize = reader.size();
    if (sourceSize.isValid() && sourceSize.width() > kBackdropWidth)
        reader.setScaledSize(sourceSize.scaled(kBackdropWidth, kBackdropWidth * 4,
                                               Qt::KeepAspectRatio));
    QImage image = reader.read();
    if (image.isNull())
        return;
    // Down- then up-sampling is a cheap, wide blur matching the compositor's
    // frosted backdrop closely enough at thumbnail size.
    const QSize full = image.size();
    image = image.scaledToWidth(kBackdropBlurWidth, Qt::SmoothTransformation)
                .scaled(full, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_backdrop = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

QImage DockWindowPreviewService::composeOverBackdrop(const QImage &image) const
{
    if (m_backdrop.isNull() || !image.hasAlphaChannel())
        return image;
    QImage composed(image.size(), QImage::Format_ARGB32_Premultiplied);
    composed.fill(Qt::transparent);
    QPainter painter(&composed);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QSize cover = m_backdrop.size().scaled(image.size(), Qt::KeepAspectRatioByExpanding);
    painter.drawImage(QRect(QPoint((image.width() - cover.width()) / 2,
                                   (image.height() - cover.height()) / 2),
                            cover),
                      m_backdrop);
    painter.drawImage(0, 0, image);
    painter.end();
    return composed;
}

QImage DockWindowPreviewService::imageForWindow(const QString &windowId)
{
    QMutexLocker lock(&m_mutex);
    auto it = m_cache.find(windowId);
    if (it == m_cache.end() || it->generation != m_currentGeneration)
        return {};
    it->lastUse = ++m_useSerial;
    return it->image;
}

void DockWindowPreviewService::requestNext()
{
    QString windowId;
    quint64 token = 0;
    {
        QMutexLocker lock(&m_mutex);
        if (m_requestInFlight || m_queue.isEmpty() || !m_client)
            return;
        if (m_hasLiveWindowSet) {
            while (!m_queue.isEmpty() && !m_liveWindowIds.contains(m_queue.head()))
                m_queue.dequeue();
        }
        if (m_queue.isEmpty())
            return;
        windowId = m_queue.dequeue();
        m_activeWindowId = windowId;
        m_activeToken = token = ++m_nextToken;
        m_requestInFlight = true;
    }
    if (!m_client->isReady()) {
        QMutexLocker lock(&m_mutex);
        if (m_requestInFlight && m_activeToken == token) {
            m_requestInFlight = false;
            m_activeWindowId.clear();
            m_queue.prepend(windowId);
        }
        return;
    }
    if (!m_client->supportsWindowCapture()) {
        finishRequest(QStringLiteral("unsupported"), {}, m_currentGeneration, token);
        return;
    }
    const bool queued = m_client->requestWindowCapture(windowId, kCaptureWidth, kCaptureHeight,
                                                       token);
    if (!queued) {
        QMutexLocker lock(&m_mutex);
        if (m_requestInFlight && m_activeToken == token) {
            m_requestInFlight = false;
            m_activeWindowId.clear();
        }
    }
}

void DockWindowPreviewService::finishRequest(const QString &reason, QImage image,
                                            quint64 generation, quint64 token)
{
    QString completedWindow;
    bool completed = false;
    {
        QMutexLocker lock(&m_mutex);
        if (!m_requestInFlight || token != m_activeToken)
            return;
        completedWindow = m_activeWindowId;
        m_requestInFlight = false;
        m_activeWindowId.clear();
        completed = true;
    }
    if (!completed)
        return;
    if (reason.isEmpty() && !image.isNull() && generation == m_currentGeneration)
        cacheImage(completedWindow, composeOverBackdrop(image), generation);
    requestNext();
}

void DockWindowPreviewService::cacheImage(const QString &windowId, const QImage &image,
                                          quint64 generation)
{
    if (image.isNull())
        return;
    const qsizetype bytes = static_cast<qsizetype>(image.sizeInBytes());
    if (bytes <= 0 || bytes > kMaximumCacheBytes)
        return;
    {
        QMutexLocker lock(&m_mutex);
        auto old = m_cache.find(windowId);
        if (old != m_cache.end())
            m_cachedBytes -= old->bytes;
        CachedImage entry;
        entry.image = image;
        entry.generation = generation;
        entry.revision = ++m_revisionSerial;
        entry.lastUse = ++m_useSerial;
        entry.bytes = bytes;
        m_cache.insert(windowId, std::move(entry));
        m_cachedBytes += bytes;
        evictToLimit();
    }
    emit imageChanged(windowId);
}

void DockWindowPreviewService::evictToLimit()
{
    while (m_cachedBytes > kMaximumCacheBytes && !m_cache.isEmpty()) {
        auto oldest = m_cache.begin();
        for (auto it = std::next(m_cache.begin()); it != m_cache.end(); ++it) {
            if (it->lastUse < oldest->lastUse)
                oldest = it;
        }
        m_cachedBytes -= oldest->bytes;
        m_cache.erase(oldest);
    }
}
