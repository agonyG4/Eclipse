#include "WallpaperSurfaceManager.hpp"

#include "WallpaperSurfaceBundle.hpp"
#include "core/WallpaperService.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QScreen>

#include <utility>

namespace Paper {

WallpaperSurfaceManager::WallpaperSurfaceManager(QGuiApplication &application,
                                                 QQmlApplicationEngine &engine,
                                                 WallpaperService *service,
                                                 QObject *parent,
                                                 BundleFactory bundleFactory)
    : QObject(parent)
    , m_application(application)
    , m_engine(engine)
    , m_service(service)
    , m_bundleFactory(std::move(bundleFactory))
{
    if (!m_bundleFactory) {
        m_bundleFactory = [this](QScreen *screen, QObject *parentObject) {
            return new WallpaperSurfaceBundle(screen, &m_engine, m_service, parentObject);
        };
    }
    connect(&m_application, &QGuiApplication::screenAdded, this, [this](QScreen *screen) {
        QString error;
        if (!addScreen(screen, &error)) {
            qWarning("Paper wallpaper output initialization failed: %s", qPrintable(error));
        }
    });
    connect(&m_application, &QGuiApplication::screenRemoved,
            this, &WallpaperSurfaceManager::removeScreen);
    if (m_service) {
        connect(m_service,
                &WallpaperService::effectiveWallpaperChanged,
                this,
                &WallpaperSurfaceManager::handleEffectiveWallpaperChanged);
    }
}

WallpaperSurfaceManager::~WallpaperSurfaceManager()
{
    shutdown();
}

bool WallpaperSurfaceManager::initialize(QString *errorOut)
{
    if (m_initialized) {
        return true;
    }
    if (!m_service) {
        if (errorOut) {
            *errorOut = QStringLiteral("Paper wallpaper surface manager requires a service");
        }
        return false;
    }
    m_initialized = true;
    for (QScreen *screen : m_application.screens()) {
        if (!addScreen(screen, errorOut)) {
            shutdown();
            return false;
        }
    }
    return true;
}

void WallpaperSurfaceManager::shutdown()
{
    const auto bundles = m_bundles;
    for (QScreen *screen : bundles.keys()) {
        QObject::disconnect(screen, &QScreen::geometryChanged, this, nullptr);
    }
    m_bundles.clear();
    for (auto *bundle : bundles) {
        delete bundle;
    }
    if (!bundles.isEmpty()) {
        emit bundleCountChanged();
    }
    m_initialized = false;
}

void WallpaperSurfaceManager::handleEffectiveWallpaperChanged()
{
    if (!m_service) {
        return;
    }
    for (auto *bundle : m_bundles) {
        if (bundle) {
            bundle->setEffectiveWallpaper(m_service->snapshot().effective);
        }
    }
}

bool WallpaperSurfaceManager::addScreen(QScreen *screen, QString *errorOut)
{
    if (!screen || m_bundles.contains(screen)) {
        return true;
    }
    auto *bundle = m_bundleFactory(screen, this);
    if (!bundle) {
        if (errorOut) {
            *errorOut = QStringLiteral("Paper wallpaper bundle factory returned null");
        }
        return false;
    }
    if (!bundle->initialize(errorOut)) {
        delete bundle;
        return false;
    }
    const QPointer<QScreen> trackedScreen(screen);
    connect(screen, &QScreen::geometryChanged, this, [this, trackedScreen](const QRect &) {
        handleScreenGeometryChanged(trackedScreen.data());
    });
    bundle->setEffectiveWallpaper(m_service->snapshot().effective);
    m_bundles.insert(screen, bundle);
    bundle->map();
    emit bundleCountChanged();
    return true;
}

void WallpaperSurfaceManager::removeScreen(QScreen *screen)
{
    auto it = m_bundles.find(screen);
    if (it == m_bundles.end()) {
        return;
    }
    QObject::disconnect(screen, &QScreen::geometryChanged, this, nullptr);
    auto *bundle = it.value();
    m_bundles.erase(it);
    delete bundle;
    emit bundleCountChanged();
}

void WallpaperSurfaceManager::handleScreenGeometryChanged(QScreen *screen)
{
    if (auto it = m_bundles.constFind(screen); it != m_bundles.constEnd() && it.value()) {
        it.value()->updateForScreen();
    }
}

} // namespace Paper
