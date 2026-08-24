#include "WallpaperSurfaceBundle.hpp"

#include "core/WallpaperService.hpp"
#include "WallpaperSurfacePolicy.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QUrl>
#include <QVariantMap>

namespace Paper {
namespace {

QString imageUrl(const WallpaperDescriptor &descriptor)
{
    const auto source = descriptor.resolvedSource().isEmpty() ? descriptor.source()
                                                                : descriptor.resolvedSource();
    if (source.startsWith(QStringLiteral(":/")) || source.startsWith(QStringLiteral("qrc:/"))) {
        return source.startsWith(QStringLiteral(":/"))
            ? QStringLiteral("qrc") + source
            : source;
    }
    return QUrl::fromLocalFile(source).toString();
}

} // namespace

WallpaperSurfaceBundle::WallpaperSurfaceBundle(QScreen *screen,
                                               QQmlApplicationEngine *engine,
                                               WallpaperService *service,
                                               QObject *parent)
    : QObject(parent)
    , m_screen(screen)
    , m_engine(engine)
    , m_service(service)
{
}

WallpaperSurfaceBundle::~WallpaperSurfaceBundle()
{
    destroySurface();
}

bool WallpaperSurfaceBundle::initialize(QString *errorOut)
{
    if (m_layerConfigured) {
        return true;
    }
    if (!m_screen || !m_engine || !m_service) {
        if (errorOut) {
            *errorOut = QStringLiteral("Paper wallpaper bundle requires screen, engine, and service");
        }
        return false;
    }

    QQmlComponent component(m_engine,
                            QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Paper/qml/WallpaperSurface.qml")),
                            this);
    if (component.status() != QQmlComponent::Ready) {
        if (errorOut) {
            *errorOut = component.errors().isEmpty()
                ? QStringLiteral("Paper wallpaper QML component is not ready")
                : component.errors().constFirst().toString();
        }
        return false;
    }

    const auto geometry = m_screen->geometry();
    const auto &effective = m_service->snapshot().effective;
    QVariantMap properties{
        {QStringLiteral("wallpaperSource"), imageUrl(effective)},
        {QStringLiteral("wallpaperFit"), wallpaperFitToString(effective.fit())},
        {QStringLiteral("wallpaperGeneration"),
         static_cast<qulonglong>(m_service->snapshot().generation)},
        {QStringLiteral("outputWidth"), qMax(1, geometry.width())},
        {QStringLiteral("outputHeight"), qMax(1, geometry.height())},
    };
    QObject *object = component.createWithInitialProperties(properties, m_engine->rootContext());
    auto *quickWindow = qobject_cast<QQuickWindow *>(object);
    if (!quickWindow) {
        if (object) {
            object->deleteLater();
        }
        if (errorOut) {
            *errorOut = QStringLiteral("Paper wallpaper QML component did not create a window");
        }
        return false;
    }
    QQmlEngine::setObjectOwnership(quickWindow, QQmlEngine::CppOwnership);
    quickWindow->setScreen(m_screen.data());
    quickWindow->setFlag(Qt::WindowTransparentForInput, true);
    quickWindow->setFlag(Qt::WindowDoesNotAcceptFocus, true);
    quickWindow->setVisible(false);
    quickWindow->resize(qMax(1, geometry.width()), qMax(1, geometry.height()));

    const auto config = WallpaperSurfacePolicy::background(m_screen.data());
    if (!AstreaLayerShellHelper::configure(quickWindow, config, errorOut)) {
        delete quickWindow;
        return false;
    }

    m_window = quickWindow;
    m_layerConfigured = true;
    updateForScreen();
    return true;
}

void WallpaperSurfaceBundle::map()
{
    if (!m_layerConfigured || m_mapped) {
        return;
    }
    if (m_window) {
        m_window->show();
    }
    m_mapped = true;
}

void WallpaperSurfaceBundle::setEffectiveWallpaper(const WallpaperDescriptor &descriptor)
{
    if (!m_window) {
        return;
    }
    m_window->setProperty("wallpaperSource", imageUrl(descriptor));
    m_window->setProperty("wallpaperFit", wallpaperFitToString(descriptor.fit()));
    if (m_service) {
        m_window->setProperty("wallpaperGeneration",
                              static_cast<qulonglong>(m_service->snapshot().generation));
    }
}

void WallpaperSurfaceBundle::updateForScreen()
{
    if (!m_screen || !m_window) {
        return;
    }
    const auto geometry = m_screen->geometry();
    m_window->setScreen(m_screen.data());
    m_window->setProperty("outputWidth", qMax(1, geometry.width()));
    m_window->setProperty("outputHeight", qMax(1, geometry.height()));
    m_window->resize(qMax(1, geometry.width()), qMax(1, geometry.height()));
}

void WallpaperSurfaceBundle::destroySurface()
{
    m_mapped = false;
    m_layerConfigured = false;
    if (!m_window) {
        return;
    }
    m_window->setVisible(false);
    m_window->close();
    delete m_window.data();
    m_window.clear();
}

} // namespace Paper
