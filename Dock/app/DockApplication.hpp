#pragma once

#include "app/CommandLine.hpp"
#include "core/DockController.hpp"
#include "platform/ipc/DockIpcServer.hpp"
#include "platform/runtime/DockRuntimePaths.hpp"
#include "services/DockConfigWatcher.hpp"

#include <QPointer>
#include <QQmlApplicationEngine>

#include <memory>

class QGuiApplication;
class QQuickWindow;
class AstreaIconProvider;
class DesktopEntryCatalog;

class DockApplication final : public QObject {
    Q_OBJECT

public:
    explicit DockApplication(QGuiApplication &app);
    int run();

private:
    bool initializeRuntime();
    bool initializeServices();
    bool initializeQml();
    void connectSignals();
    int runClientCommand(const CommandLineRequest &request);
    void configureLayerShell();
    void updateLayerMapping();
    QString buildStatusJson() const;

    QGuiApplication &m_app;
    CommandLineRequest m_request;
    std::unique_ptr<DockRuntimePaths> m_paths;
    std::unique_ptr<DesktopEntryCatalog> m_catalog;
    std::unique_ptr<ApplicationLauncher> m_launcher;
    std::unique_ptr<DockController> m_controller;
    std::unique_ptr<DockConfigWatcher> m_configWatcher;
    std::unique_ptr<DockIpcServer> m_ipcServer;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    QPointer<QQuickWindow> m_window;
    AstreaIconProvider *m_iconProvider = nullptr;

    struct LayerShellState {
        bool compiled = false;
        bool configured = false;
        bool mapped = false;
        int exclusiveZone = 0;
        QString namespaceName = QStringLiteral("astrea-dock");
        QString screen;
        QString error;
    } m_layerState;
};
