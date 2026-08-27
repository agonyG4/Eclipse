#pragma once

#include <QObject>
#include <QPointer>
#include <QScreen>

namespace Astrea::Shell {
class ContextMenuController;
}
class QQuickWindow;
class QQmlApplicationEngine;
namespace Astrea::StatusNotifier {
class StatusNotifierService;
}

class ContextMenuSurfaceBundle final : public QObject {
    Q_OBJECT

public:
    ContextMenuSurfaceBundle(QScreen *screen, QQmlApplicationEngine *engine,
                             Astrea::Shell::ContextMenuController *controller,
                             QObject *parent = nullptr,
                             Astrea::StatusNotifier::StatusNotifierService *statusNotifier = nullptr);
    ~ContextMenuSurfaceBundle() override;

    bool initialize(QString *errorOut = nullptr);
    void map();
    void updateForScreen();
    void destroySurfaces();

    QScreen *screen() const { return m_screen.data(); }
    QString outputKey() const;
    int surfaceCount() const;
    bool overlayMapped() const;
    bool layerConfigurationRequested() const { return m_layerConfigurationRequested; }

private:
    QQuickWindow *createSurface(const QUrl &sourceUrl, int width, int height,
                                QString *errorOut);
    bool configureSurface(QQuickWindow *window, bool overlay, QString *errorOut);
    void syncOverlayMapping();
    void destroyWindow(QPointer<QQuickWindow> &window);

    QPointer<QScreen> m_screen;
    QQmlApplicationEngine *m_engine = nullptr;
    Astrea::Shell::ContextMenuController *m_controller = nullptr;
    Astrea::StatusNotifier::StatusNotifierService *m_statusNotifier = nullptr;
    QPointer<QQuickWindow> m_desktopInteractionWindow;
    QPointer<QQuickWindow> m_overlayWindow;
    bool m_layerConfigurationRequested = false;
    bool m_mapped = false;
};
