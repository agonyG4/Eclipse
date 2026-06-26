#pragma once

#include "app/CommandLine.hpp"
#include "core/AltTabController.hpp"
#include "services/AppIdentityResolver.hpp"
#include "services/AltTabConfigWatcher.hpp"
#include "platform/ipc/AltTabIpcServer.hpp"
#include "platform/compositor/CompositorBackend.hpp"
#include "platform/runtime/AltTabRuntimePaths.hpp"

#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <memory>

class QGuiApplication;
class QQuickWindow;
class AstreaIconProvider;

class AltTabApplication final : public QObject {
    Q_OBJECT

public:
    explicit AltTabApplication(QGuiApplication &app);
    int run(int argc, char **argv);

private:
    bool initializeRuntime();
    bool initializeServices();
    bool initializeQml();
    void connectSignals();
    int runClientCommand(const CommandLineRequest &request);
    QString buildStatusJson() const;
    void placeOverlayOnFocusedScreen();

    QGuiApplication &m_app;
    CommandLineRequest m_request;
    std::unique_ptr<AltTabRuntimePaths> m_paths;
    std::unique_ptr<CompositorBackend> m_backend;
    std::unique_ptr<AppIdentityResolver> m_identityResolver;
    std::unique_ptr<AltTabController> m_controller;
    std::unique_ptr<AltTabConfigWatcher> m_configWatcher;
    std::unique_ptr<AltTabIpcServer> m_ipcServer;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    AstreaIconProvider *m_iconProvider = nullptr;
    QPointer<QQuickWindow> m_overlayWindow;

    struct LayerShellState {
        bool compiled = false;
        bool configured = false;
        QString error;
    } m_layerState;
};
