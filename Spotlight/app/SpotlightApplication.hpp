#pragma once

#include "app/CommandLine.hpp"
#include "core/SpotlightController.hpp"
#include "platform/icons/AstreaIconProvider.hpp"
#include "platform/ipc/SpotlightIpcServer.hpp"
#include "platform/runtime/SpotlightRuntimePaths.hpp"
#include "services/AstreaI18n.hpp"
#include "services/GameModeMonitor.hpp"
#include "services/SpotlightConfigWatcher.hpp"

#include <QObject>
#include <QString>
#include <QQmlApplicationEngine>
#include <QFileSystemWatcher>
#include <QTimer>
#include <memory>

class QGuiApplication;

class SpotlightApplication final : public QObject {
    Q_OBJECT

public:
    explicit SpotlightApplication(QGuiApplication &app);
    int run(int argc, char **argv);

private:
    bool initializeRuntime();
    bool initializeBackend();
    bool initializeServices();
    bool initializeQml();
    void connectSignals();
    int runClientCommand(const CommandLineRequest &request);
    void updateApplicationDirectoryWatchers();
    QString buildStatusJson() const;

    QGuiApplication &m_app;
    CommandLineRequest m_request;
    std::unique_ptr<SpotlightRuntimePaths> m_paths;
    std::unique_ptr<AstreaI18n> m_i18n;
    std::unique_ptr<SpotlightController> m_controller;
    std::unique_ptr<SpotlightConfigWatcher> m_configWatcher;
    std::unique_ptr<GameModeMonitor> m_gameMode;
    std::unique_ptr<SpotlightIpcServer> m_ipcServer;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    std::unique_ptr<QFileSystemWatcher> m_appWatcher;
    std::unique_ptr<QTimer> m_appRefreshDebounce;
    AstreaIconProvider *m_iconProvider = nullptr;
    struct LayerShellState {
        bool compiled = false;
        bool configured = false;
        QString error;
    } m_layerState;
};
