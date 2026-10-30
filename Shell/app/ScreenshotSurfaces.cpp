#include "app/AstreaShellApplication.hpp"

#include "platform/wayland/LayerShellHelper.hpp"
#include "platform/wayland/TransientQmlWindowSlot.hpp"
#include "screenshot/ScreenshotController.hpp"
#include "screenshot/ScreenshotInputRegionBridge.hpp"
#include "screenshot/ScreenshotUiBarrier.hpp"
#include "runtime/ShellRuntime.hpp"

#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>

void AstreaShellApplication::syncScreenshotOverlaySurface()
{
    if (!m_runtime || !m_engine || !m_screenshotOverlaySlot)
        return;
    auto *controller = m_runtime->screenshotController();
    if (!controller->isVisible()) {
        m_screenshotOverlaySlot->hide();
        QTimer::singleShot(0, this, [this] {
            if (!m_runtime || !m_screenshotOverlaySlot)
                return;
            auto *current = m_runtime->screenshotController();
            if (current->isVisible()
                || current->phase() == ScreenshotPhase::WaitingLiveCapture)
                return;
            m_screenshotOverlaySlot->destroyWhenSafe();
        });
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    AstreaLayerShellConfig config;
    config.scope = QStringLiteral("astrea-screenshot");
    config.layer = AstreaLayerShellConfig::Layer::Overlay;
    config.keyboardInteractivity = AstreaLayerShellConfig::KeyboardInteractivity::Exclusive;
    config.anchorTop = true;
    config.anchorBottom = true;
    config.anchorLeft = true;
    config.anchorRight = true;
    config.exclusiveZone = -1;
    config.screen = screen;

    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = QUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Shell/Screenshot/ScreenshotOverlay.qml"));
    request.screen = screen;
    request.size = screen->geometry().size();
    request.requireSurfaceReadyGate = true;
    request.configure = [config](QQuickWindow *window, QString *error) {
        return AstreaLayerShellHelper::configure(window, config, error);
    };
    request.install = [this](QQuickWindow *, QString *) {
        refreshScreenshotBarrierWindows();
        return true;
    };
    request.beforeDestroy = [this](QQuickWindow *window) {
        refreshScreenshotBarrierWindows(window);
    };
    QString error;
    if (!m_screenshotOverlaySlot->ensureCreated(m_engine.get(), request, &error)) {
        qCritical("Astrea shell screenshot surface creation failed: %s", qPrintable(error));
        m_application.exit(1);
    }
}

void AstreaShellApplication::syncScreenshotThumbnailSurface()
{
    if (!m_runtime || !m_engine || !m_screenshotThumbnailSlot)
        return;
    auto *controller = m_runtime->screenshotController();
    if (!controller->thumbnailVisible()) {
        if (controller->thumbnailDragActive())
            return;
        m_screenshotThumbnailSlot->hide();
        QTimer::singleShot(0, this, [this] {
            if (!m_runtime || !m_screenshotThumbnailSlot)
                return;
            auto *current = m_runtime->screenshotController();
            if (current->thumbnailVisible() || current->thumbnailDragActive())
                return;
            m_screenshotThumbnailSlot->destroyWhenSafe();
        });
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    AstreaLayerShellConfig config;
    config.scope = QStringLiteral("astrea-screenshot-thumbnail");
    config.layer = AstreaLayerShellConfig::Layer::Overlay;
    config.keyboardInteractivity = AstreaLayerShellConfig::KeyboardInteractivity::None;
    config.anchorTop = true;
    config.anchorBottom = true;
    config.anchorLeft = true;
    config.anchorRight = true;
    config.exclusiveZone = -1;
    config.screen = screen;

    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = QUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Shell/Screenshot/ScreenshotThumbnail.qml"));
    request.screen = screen;
    request.size = screen->geometry().size();
    request.requireSurfaceReadyGate = true;
    request.configure = [config](QQuickWindow *window, QString *error) {
        return AstreaLayerShellHelper::configure(window, config, error);
    };
    request.install = [this](QQuickWindow *window, QString *) {
        m_screenshotInputRegion->setWindow(window);
        refreshScreenshotBarrierWindows();
        return true;
    };
    request.beforeDestroy = [this](QQuickWindow *window) {
        m_screenshotInputRegion->setWindow(nullptr);
        refreshScreenshotBarrierWindows(window);
    };
    QString error;
    if (!m_screenshotThumbnailSlot->ensureCreated(m_engine.get(), request, &error)) {
        qCritical("Astrea shell screenshot thumbnail creation failed: %s", qPrintable(error));
        m_application.exit(1);
    }
}

void AstreaShellApplication::syncScreenshotPreviewSurface()
{
    if (!m_runtime || !m_engine || !m_screenshotPreviewSlot)
        return;
    if (!m_runtime->screenshotController()->previewVisible()) {
        m_screenshotPreviewSlot->hide();
        QTimer::singleShot(0, this, [this] {
            if (!m_runtime || !m_screenshotPreviewSlot
                || m_runtime->screenshotController()->previewVisible())
                return;
            m_screenshotPreviewSlot->destroyWhenSafe();
        });
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    AstreaLayerShellConfig config;
    config.scope = QStringLiteral("astrea-screenshot-preview");
    config.layer = AstreaLayerShellConfig::Layer::Overlay;
    config.keyboardInteractivity = AstreaLayerShellConfig::KeyboardInteractivity::Exclusive;
    config.anchorTop = true;
    config.anchorBottom = true;
    config.anchorLeft = true;
    config.anchorRight = true;
    config.exclusiveZone = -1;
    config.screen = screen;

    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = QUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Shell/Screenshot/ScreenshotPreview.qml"));
    request.screen = screen;
    request.size = screen->geometry().size();
    request.requireSurfaceReadyGate = true;
    request.configure = [config](QQuickWindow *window, QString *error) {
        return AstreaLayerShellHelper::configure(window, config, error);
    };
    request.install = [this](QQuickWindow *, QString *) {
        refreshScreenshotBarrierWindows();
        return true;
    };
    request.beforeDestroy = [this](QQuickWindow *window) {
        refreshScreenshotBarrierWindows(window);
    };
    QString error;
    if (!m_screenshotPreviewSlot->ensureCreated(m_engine.get(), request, &error)) {
        qCritical("Astrea shell screenshot preview creation failed: %s", qPrintable(error));
        m_application.exit(1);
    }
}

void AstreaShellApplication::refreshScreenshotBarrierWindows(QQuickWindow *excluded)
{
    if (!m_screenshotUiBarrier)
        return;
    auto currentWindow = [excluded](const std::unique_ptr<TransientQmlWindowSlot> &slot) {
        QQuickWindow *window = slot ? slot->window() : nullptr;
        return window == excluded ? nullptr : window;
    };
    m_screenshotUiBarrier->setTrackedWindows({
        currentWindow(m_screenshotOverlaySlot),
        currentWindow(m_screenshotThumbnailSlot),
        currentWindow(m_screenshotPreviewSlot),
    });
}
