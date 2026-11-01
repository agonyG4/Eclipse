#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QMutex>
#include <QStringList>
#include <QQueue>

#include <cstdint>

class TyphonScreenCaptureClient;

class DockWindowPreviewService final : public QObject {
    Q_OBJECT

public:
    explicit DockWindowPreviewService(TyphonScreenCaptureClient *client,
                                      QObject *parent = nullptr);

    Q_INVOKABLE void requestWindowPreview(const QString &windowId, bool refresh = false);
    Q_INVOKABLE QString imageUrl(const QString &windowId) const;
    Q_INVOKABLE void reconcileLiveWindowIds(const QStringList &windowIds);
    Q_INVOKABLE void setMinimizedWindowIds(const QStringList &windowIds);
    Q_INVOKABLE void setPopupWindowIds(const QStringList &windowIds);
    // Translucent windows are previewed over this image, blurred, the way they
    // read on the desktop instead of over an empty dark card.
    void setBackdropSource(const QString &source);
    QImage imageForWindow(const QString &windowId);

signals:
    void imageChanged(QString windowId);
    void generationChanged(quint64 generation);

private:
    struct CachedImage {
        QImage image;
        quint64 generation = 0;
        quint64 revision = 0;
        quint64 lastUse = 0;
        qsizetype bytes = 0;
    };

    void requestNext();
    void finishRequest(const QString &reason, QImage image, quint64 generation,
                       quint64 token);
    void cacheImage(const QString &windowId, const QImage &image, quint64 generation);
    void evictToLimit();
    void cancelRequestsWithoutVisibleDemand();
    QImage composeOverBackdrop(const QImage &image) const;

    TyphonScreenCaptureClient *m_client = nullptr;
    mutable QMutex m_mutex;
    QHash<QString, CachedImage> m_cache;
    QStringList m_liveWindowIds;
    QStringList m_minimizedWindowIds;
    QStringList m_popupWindowIds;
    QQueue<QString> m_queue;
    QString m_backdropSource;
    QImage m_backdrop;
    QString m_activeWindowId;
    quint64 m_activeToken = 0;
    quint64 m_nextToken = 0;
    quint64 m_currentGeneration = 0;
    quint64 m_useSerial = 0;
    quint64 m_revisionSerial = 0;
    qsizetype m_cachedBytes = 0;
    bool m_requestInFlight = false;
    bool m_hasLiveWindowSet = false;
};
