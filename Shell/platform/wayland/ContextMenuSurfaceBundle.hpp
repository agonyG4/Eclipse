#pragma once

#include "platform/wayland/TransientQmlWindowSlot.hpp"

#include <QObject>
#include <QPointer>
#include <QScreen>
#include <QVariantMap>

namespace Astrea::Shell {
class ContextMenuController;
}
class QQuickWindow;
class QQmlApplicationEngine;
class QUrl;
namespace Astrea::StatusNotifier {
class StatusNotifierService;
}

class ContextMenuSurfaceBundle : public QObject {
    Q_OBJECT

public:
    ContextMenuSurfaceBundle(QScreen *screen, QQmlApplicationEngine *engine,
                             Astrea::Shell::ContextMenuController *controller,
                             QObject *parent = nullptr,
                             Astrea::StatusNotifier::StatusNotifierService *statusNotifier = nullptr);
    ~ContextMenuSurfaceBundle() override;

    virtual bool initialize(QString *errorOut = nullptr);
    virtual void map();
    virtual void updateForScreen();
    virtual void destroySurfaces();

    QScreen *screen() const { return m_screen.data(); }
    virtual QString outputKey() const;
    int surfaceCount() const;
    virtual bool overlayMapped() const;
    QVariantMap overlayResidency() const { return m_overlaySlot.residencySnapshot(); }
    bool layerConfigurationRequested() const { return m_layerConfigurationRequested; }

signals:
    void mappingChanged();
    void surfaceError(const QString &error);

private:
    QQuickWindow *createSurface(const QUrl &sourceUrl, const QVariantMap &properties,
                                QString *errorOut);
    QQuickWindow *createOverlaySurface(QString *errorOut = nullptr);
    void syncOverlayMapping();
    void destroyWindow(QPointer<QQuickWindow> &window);

protected:
    virtual bool configureSurface(QQuickWindow *window, bool overlay, QString *errorOut);

private:
    QPointer<QScreen> m_screen;
    QQmlApplicationEngine *m_engine = nullptr;
    Astrea::Shell::ContextMenuController *m_controller = nullptr;
    Astrea::StatusNotifier::StatusNotifierService *m_statusNotifier = nullptr;
    QPointer<QQuickWindow> m_desktopInteractionWindow;
    TransientQmlWindowSlot m_overlaySlot;
    bool m_layerConfigurationRequested = false;
    bool m_mapped = false;
};
