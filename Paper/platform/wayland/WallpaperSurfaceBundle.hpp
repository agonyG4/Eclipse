#pragma once

#include "core/WallpaperDescriptor.hpp"

#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QScreen>

class QQmlApplicationEngine;

namespace Paper {

class WallpaperService;

class WallpaperSurfaceBundle : public QObject
{
    Q_OBJECT

public:
    WallpaperSurfaceBundle(QScreen *screen,
                           QQmlApplicationEngine *engine,
                           WallpaperService *service,
                           QObject *parent = nullptr);
    ~WallpaperSurfaceBundle() override;

    virtual bool initialize(QString *errorOut = nullptr);
    virtual void map();
    virtual void setEffectiveWallpaper(const WallpaperDescriptor &descriptor);
    virtual void updateForScreen();
    virtual void destroySurface();

    QScreen *screen() const { return m_screen.data(); }
    bool layerConfigured() const { return m_layerConfigured; }
    bool mapped() const { return m_mapped; }

protected:
    QQuickWindow *window() const { return m_window.data(); }

private:
    QPointer<QScreen> m_screen;
    QQmlApplicationEngine *m_engine = nullptr;
    WallpaperService *m_service = nullptr;
    QPointer<QQuickWindow> m_window;
    bool m_layerConfigured = false;
    bool m_mapped = false;
};

} // namespace Paper
