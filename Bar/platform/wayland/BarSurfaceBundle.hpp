#pragma once

#include "core/BarSurfacePolicy.hpp"

#include <QObject>
#include <QPointer>
#include <QScreen>
#include <QUrl>

class BarClockService;
class BarController;
class BarLayoutMetrics;
class BarPopupController;
class QQuickWindow;
class QQmlApplicationEngine;
class WorkspaceModel;

class BarSurfaceBundle : public QObject {
    Q_OBJECT

public:
    BarSurfaceBundle(QScreen *screen, QQmlApplicationEngine *engine,
                     BarController *barController, BarClockService *clockService,
                     WorkspaceModel *workspaceModel, QObject *parent = nullptr);
    ~BarSurfaceBundle() override;

    virtual bool initialize(QString *errorOut = nullptr);
    virtual void map();
    virtual void setBarEnabled(bool enabled);
    virtual void destroySurfaces();
    virtual void updateForScreen();

    QScreen *screen() const { return m_screen.data(); }
    virtual int surfaceCount() const;
    virtual bool popupOpen() const;
    virtual bool layerConfigurationRequested() const { return m_layerConfigurationRequested; }
    BarPopupController *popupController() const { return m_popupController; }
    BarLayoutMetrics *layoutMetrics() const { return m_layoutMetrics; }

signals:
    void popupStateChanged();

private:
    QQuickWindow *createSurface(const QUrl &sourceUrl, int width, int height,
                                QString *errorOut, bool sizeWindow);
    bool configureSurface(QQuickWindow *window, BarSurfaceKind kind, QString *errorOut);
    void syncPopupMapping();
    void destroyWindow(QPointer<QQuickWindow> &window);

    QPointer<QScreen> m_screen;
    QQmlApplicationEngine *m_engine = nullptr;
    BarController *m_barController = nullptr;
    BarClockService *m_clockService = nullptr;
    WorkspaceModel *m_workspaceModel = nullptr;
    BarPopupController *m_popupController = nullptr;
    BarLayoutMetrics *m_layoutMetrics = nullptr;
    QPointer<QQuickWindow> m_reserveWindow;
    QPointer<QQuickWindow> m_launcherWindow;
    QPointer<QQuickWindow> m_statusWindow;
    QPointer<QQuickWindow> m_popupWindow;
    bool m_layerConfigurationRequested = false;
    bool m_mapped = false;
    bool m_barEnabled = true;
};
