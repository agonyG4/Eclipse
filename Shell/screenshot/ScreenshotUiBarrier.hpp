#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QList>

#include <cstdint>
#include <functional>
#include <vector>

class QQuickWindow;
struct wl_callback;
struct wl_display;

class ScreenshotUiBarrier {
public:
    using Completion = std::function<void(bool success, QString reason)>;

    virtual ~ScreenshotUiBarrier() = default;
    virtual void synchronize(Completion completion) = 0;
};

class ScreenshotWaylandUiBarrier final : public QObject, public ScreenshotUiBarrier {
    Q_OBJECT

public:
    explicit ScreenshotWaylandUiBarrier(QObject *parent = nullptr);
    ~ScreenshotWaylandUiBarrier() override;

    void setTrackedWindows(const QList<QQuickWindow *> &windows);
    void synchronize(Completion completion) override;
    static void callbackDone(void *data, wl_callback *callback, std::uint32_t callbackData);

private:
    void checkVisibility();
    void issueWaylandSyncIfStillHidden();
    void complete(bool success, const QString &reason = {});

    QPointer<QQuickWindow> m_selectionWindow;
    QPointer<QQuickWindow> m_thumbnailWindow;
    QPointer<QQuickWindow> m_previewWindow;
    std::vector<QMetaObject::Connection> m_windowConnections;
    ScreenshotUiBarrier::Completion m_completion;
    wl_display *m_display = nullptr;
    wl_callback *m_callback = nullptr;
    bool m_syncScheduled = false;
};
