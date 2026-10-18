#pragma once

#include "platform/wayland/BarSurfacePolicy.hpp"
#include "platform/wayland/TransientQmlWindowSlot.hpp"

#include <QObject>
#include <QPointer>
#include <QScreen>
#include <QUrl>
#include <QVariantMap>

class BarClockService;
class BarController;
class BarLayoutMetrics;
class BarPopupController;
class QQuickWindow;
class QQmlApplicationEngine;
class WorkspaceModel;
namespace Astrea::System {
class AudioService;
class BluetoothService;
class NetworkService;
}
namespace Astrea::StatusNotifier {
class StatusNotifierService;
}

class BarSurfaceBundle : public QObject {
    Q_OBJECT

public:
    BarSurfaceBundle(QScreen *screen, QQmlApplicationEngine *engine,
                     BarController *barController, BarClockService *clockService,
                     WorkspaceModel *workspaceModel, Astrea::System::AudioService *audioService,
                     Astrea::System::NetworkService *networkService,
                     Astrea::System::BluetoothService *bluetoothService,
                     QObject *parent = nullptr,
                     Astrea::StatusNotifier::StatusNotifierService *statusNotifier = nullptr,
                     QObject *contextMenuController = nullptr);
    ~BarSurfaceBundle() override;

    virtual bool initialize(QString *errorOut = nullptr);
    virtual void map();
    virtual void setBarEnabled(bool enabled);
    virtual void destroySurfaces();
    virtual void updateForScreen();

    QScreen *screen() const { return m_screen.data(); }
    virtual int surfaceCount() const;
    virtual bool popupOpen() const;
    QVariantMap popupResidency() const { return m_popupSlot.residencySnapshot(); }
    virtual bool layerConfigurationRequested() const { return m_layerConfigurationRequested; }
    BarPopupController *popupController() const { return m_popupController; }
    BarLayoutMetrics *layoutMetrics() const { return m_layoutMetrics; }

signals:
    void popupStateChanged();

private:
    QQuickWindow *createSurface(const QUrl &sourceUrl, int width, int height,
                                QString *errorOut, bool sizeWindow);
    QQuickWindow *createPopupSurface(QString *errorOut = nullptr);
    void syncPopupMapping();

protected:
    virtual bool configureSurface(QQuickWindow *window, BarSurfaceKind kind, QString *errorOut);

private slots:
    void syncTooltipMapping();

private:
    void destroyWindow(QPointer<QQuickWindow> &window);

    QPointer<QScreen> m_screen;
    QQmlApplicationEngine *m_engine = nullptr;
    BarController *m_barController = nullptr;
    BarClockService *m_clockService = nullptr;
    WorkspaceModel *m_workspaceModel = nullptr;
    Astrea::System::AudioService *m_audioService = nullptr;
    Astrea::System::NetworkService *m_networkService = nullptr;
    Astrea::System::BluetoothService *m_bluetoothService = nullptr;
    Astrea::StatusNotifier::StatusNotifierService *m_statusNotifier = nullptr;
    QPointer<QObject> m_contextMenuController;
    BarPopupController *m_popupController = nullptr;
    BarLayoutMetrics *m_layoutMetrics = nullptr;
    QPointer<QQuickWindow> m_reserveWindow;
    QPointer<QQuickWindow> m_launcherWindow;
    QPointer<QQuickWindow> m_statusWindow;
    TransientQmlWindowSlot m_popupSlot;
    QPointer<QQuickWindow> m_tooltipWindow;
    bool m_layerConfigurationRequested = false;
    bool m_mapped = false;
    bool m_barEnabled = true;
};
