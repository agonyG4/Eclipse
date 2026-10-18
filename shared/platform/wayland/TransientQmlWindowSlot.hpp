#pragma once

#include <QObject>
#include <QPointer>
#include <QSize>
#include <QUrl>
#include <QVariantMap>

#include <functional>

class QQmlEngine;
class QQuickWindow;
class QScreen;

class TransientQmlWindowSlot final : public QObject {
    Q_OBJECT

public:
    using WindowCallback = std::function<bool(QQuickWindow *, QString *)>;
    using BeforeDestroyCallback = std::function<void(QQuickWindow *)>;

    struct CreateRequest {
        QUrl sourceUrl;
        QScreen *screen = nullptr;
        QSize size;
        QVariantMap initialProperties;
        bool requireSurfaceReadyGate = false;
        WindowCallback configure;
        WindowCallback install;
        BeforeDestroyCallback beforeDestroy;
    };

    explicit TransientQmlWindowSlot(QString diagnosticId, QObject *parent = nullptr);
    ~TransientQmlWindowSlot() override;

    QQuickWindow *ensureCreated(QQmlEngine *engine, const CreateRequest &request,
                                QString *errorOut = nullptr);
    QQuickWindow *window() const;
    bool created() const;
    QVariantMap residencySnapshot() const;

    void hide();
    void cancelPendingDestruction();
    void destroyWhenSafe();
    void destroyNow();

    quint64 createCount() const { return m_createCount; }
    quint64 destroyCount() const { return m_destroyCount; }
    QString diagnosticId() const { return m_diagnosticId; }

signals:
    void residencyChanged();
    void windowDestroyed();

private:
    bool onOwnerThread(const char *operation);
    void failCreation(QQuickWindow *window, const QString &reason, QString *errorOut);

    QString m_diagnosticId;
    QPointer<QQuickWindow> m_window;
    BeforeDestroyCallback m_beforeDestroy;
    quint64 m_generation = 0;
    quint64 m_createCount = 0;
    quint64 m_destroyCount = 0;
    bool m_destroyPending = false;
};
