#pragma once

#include "core/WallpaperDescriptor.hpp"

#include <QHash>
#include <QObject>
#include <QPointer>

#include <functional>

class QGuiApplication;
class QQmlApplicationEngine;
class QScreen;

namespace Paper {

class WallpaperService;
class WallpaperSurfaceBundle;

class WallpaperSurfaceManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int bundleCount READ bundleCount NOTIFY bundleCountChanged)

public:
    using BundleFactory = std::function<WallpaperSurfaceBundle *(QScreen *, QObject *)>;

    WallpaperSurfaceManager(QGuiApplication &application,
                            QQmlApplicationEngine &engine,
                            WallpaperService *service,
                            QObject *parent = nullptr,
                            BundleFactory bundleFactory = {});
    ~WallpaperSurfaceManager() override;

    bool initialize(QString *errorOut = nullptr);
    void shutdown();
    int bundleCount() const { return m_bundles.size(); }

signals:
    void bundleCountChanged();

private slots:
    void handleEffectiveWallpaperChanged();

private:
    bool addScreen(QScreen *screen, QString *errorOut = nullptr);
    void removeScreen(QScreen *screen);
    void handleScreenGeometryChanged(QScreen *screen);

    QGuiApplication &m_application;
    QQmlApplicationEngine &m_engine;
    WallpaperService *m_service = nullptr;
    QHash<QScreen *, WallpaperSurfaceBundle *> m_bundles;
    BundleFactory m_bundleFactory;
    bool m_initialized = false;
};

} // namespace Paper
