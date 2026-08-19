#pragma once

#include "platform/ipc/ShellIpcServer.hpp"

#include <QObject>
#include <QPointer>
#include <QUrl>

#include <memory>

class AstreaI18n;
class QGuiApplication;
class QQmlApplicationEngine;
class QQuickWindow;
class ShellRuntime;
class BarSurfaceManager;

class AstreaShellApplication final : public QObject {
    Q_OBJECT

public:
    explicit AstreaShellApplication(QGuiApplication &application);
    ~AstreaShellApplication() override;

    int run();

private:
    bool initializeRuntime();
    bool listenForCommands();
    bool initializeQml();
    bool loadSurface(const QUrl &url, QQuickWindow **windowOut);
    bool configureSurfaces();
    bool configureDockSurface();
    void syncDockVisibility();
    void handleCommand(const ShellIpcServer::Command &command);
    QString statusJson() const;

    QGuiApplication &m_application;
    std::unique_ptr<ShellRuntime> m_runtime;
    std::unique_ptr<AstreaI18n> m_i18n;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    QPointer<QQuickWindow> m_dockWindow;
    QPointer<QQuickWindow> m_altTabWindow;
    QPointer<QQuickWindow> m_spotlightWindow;
    std::unique_ptr<BarSurfaceManager> m_barSurfaceManager;
    bool m_layerShellWaylandBackend = false;
    bool m_layerShellProtocolAdvertised = false;
    bool m_dockLayerConfigurationRequested = false;
    bool m_altTabLayerConfigurationRequested = false;
    bool m_spotlightLayerConfigurationRequested = false;
};
