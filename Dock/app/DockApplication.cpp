#include "app/DockApplication.hpp"

#include "icons/AstreaIconProvider.hpp"
#include "icons/AstreaIconTheme.hpp"
#include "platform/wayland/DockLayerShellSurface.hpp"
#include "platform/typhon/TyphonToplevelConnection.hpp"

#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QLocalSocket>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QTextStream>
#include <QUrl>
#include <QDebug>

namespace {
constexpr QLatin1StringView kIpcName("astrea-dock-v1");

QString typhonStateName(TyphonConnectionState state)
{
    switch (state) {
    case TyphonConnectionState::Stopped: return QStringLiteral("stopped");
    case TyphonConnectionState::Connecting: return QStringLiteral("connecting");
    case TyphonConnectionState::WaitingForRegistry: return QStringLiteral("waitingForRegistry");
    case TyphonConnectionState::WaitingForInitialSnapshot:
        return QStringLiteral("waitingForInitialSnapshot");
    case TyphonConnectionState::Ready: return QStringLiteral("ready");
    case TyphonConnectionState::Degraded: return QStringLiteral("degraded");
    case TyphonConnectionState::Disconnected: return QStringLiteral("disconnected");
    case TyphonConnectionState::Unsupported: return QStringLiteral("unsupported");
    }
    return QStringLiteral("unknown");
}
}

DockApplication::DockApplication(QGuiApplication &app)
    : QObject(&app), m_app(app)
{
}

int DockApplication::run()
{
    m_app.setApplicationName(QStringLiteral("Astrea Dock"));
    m_app.setOrganizationName(QStringLiteral("AstreaOS"));
    m_app.setQuitOnLastWindowClosed(false);
    m_request = CommandLine::parse(m_app.arguments().mid(1));
    AstreaIconTheme::apply();

    const int clientResult = runClientCommand(m_request);
    if (clientResult >= 0)
        return clientResult;
    if (!initializeRuntime() || !initializeServices())
        return 1;

    connectSignals();
    m_controller->applyConfig(m_configWatcher->config());
    m_controller->setComponentEnabled(m_configWatcher->componentEnabled());
    m_typhonConnection->start();
    if (!initializeQml())
        return 1;
    return m_app.exec();
}

int DockApplication::runClientCommand(const CommandLineRequest &request)
{
    if (request.daemonMode)
        return -1;

    DockIpcServer::IpcCommand command;
    switch (request.mode) {
    case CommandLineRequest::Mode::Status: command.type = DockIpcServer::Command::Status; break;
    case CommandLineRequest::Mode::Reload: command.type = DockIpcServer::Command::Reload; break;
    case CommandLineRequest::Mode::Show: command.type = DockIpcServer::Command::Show; break;
    case CommandLineRequest::Mode::Hide: command.type = DockIpcServer::Command::Hide; break;
    case CommandLineRequest::Mode::Quit: command.type = DockIpcServer::Command::Quit; break;
    case CommandLineRequest::Mode::Daemon: return -1;
    }

    if (request.mode == CommandLineRequest::Mode::Status) {
        const QByteArray reply = DockIpcServer::requestReply(kIpcName, command, 500);
        QTextStream out(stdout);
        if (reply.isEmpty()) {
            const QJsonObject status{{QStringLiteral("schemaVersion"), 1},
                                     {QStringLiteral("running"), false}};
            out << QString::fromUtf8(QJsonDocument(status).toJson(QJsonDocument::Compact)) << '\n';
            return 1;
        }
        out << QString::fromUtf8(reply).trimmed() << '\n';
        return 0;
    }
    return DockIpcServer::sendCommand(kIpcName, command) ? 0 : 1;
}

bool DockApplication::initializeRuntime()
{
    m_paths = std::make_unique<DockRuntimePaths>(DockRuntimePaths::fromEnvironment());
    m_catalog = std::make_unique<DesktopEntryCatalog>();
    m_catalog->initialize();
    m_launcher = std::make_unique<ApplicationLauncher>(m_paths->astreaLaunch());
    m_controller = std::make_unique<DockController>(m_launcher.get(), m_catalog.get());
    m_typhonConnection = std::make_unique<TyphonToplevelConnection>();
    return true;
}

bool DockApplication::initializeServices()
{
    m_configWatcher = std::make_unique<DockConfigWatcher>(m_paths->dockConfigPath(),
                                                          m_paths->componentsConfigPath());
    m_ipcServer = std::make_unique<DockIpcServer>();
    QString error;
    if (!m_ipcServer->listen(kIpcName, &error)) {
        qCritical("Dock IPC server already running or unavailable: %s", qPrintable(error));
        return false;
    }
    m_ipcServer->setReplyCallback([this] { return buildStatusJson(); });
    return true;
}

bool DockApplication::initializeQml()
{
    m_engine = std::make_unique<QQmlApplicationEngine>();
    connect(m_engine.get(), &QQmlApplicationEngine::warnings, this,
            [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            qWarning("Dock QML: %s", qPrintable(warning.toString()));
    });
    connect(m_engine.get(), &QQmlApplicationEngine::objectCreationFailed, this,
            [](const QUrl &url) {
        qWarning("Dock QML object creation failed: %s", qPrintable(url.toString()));
    });
    m_iconProvider = new AstreaIconProvider;
    m_engine->rootContext()->setContextProperty(QStringLiteral("AstreaIconProvider"), m_iconProvider);
    m_engine->addImageProvider(QStringLiteral("astrea-icon"), m_iconProvider);
    m_engine->rootContext()->setContextProperty(QStringLiteral("DockController"), m_controller.get());
    const QUrl qmlUrl(QStringLiteral("qrc:/qt/qml/Astrea/Dock/qml/Main.qml"));
    m_engine->load(qmlUrl);
    if (m_engine->rootObjects().isEmpty()) {
        qCritical("Dock QML root failed to load");
        return false;
    }

    m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().constFirst());
    if (!m_window) {
        qCritical("Dock QML root is not a window");
        return false;
    }
    m_layerState.compiled = DockLayerShellSurface::compiled();
    configureLayerShell();
    if (m_layerState.compiled && !m_layerState.configured)
        return false;
    connect(m_window, &QQuickWindow::heightChanged, this, [this] {
        if (m_layerState.configured)
            configureLayerShell();
    });
    updateLayerMapping();
    return true;
}

void DockApplication::connectSignals()
{
    connect(m_configWatcher.get(), &DockConfigWatcher::configChanged, this, [this] {
        m_controller->applyConfig(m_configWatcher->config());
        configureLayerShell();
    });
    connect(m_configWatcher.get(), &DockConfigWatcher::componentToggled, this,
            [this](bool enabled) { m_controller->setComponentEnabled(enabled); });
    connect(m_controller.get(), &DockController::visibleChanged, this,
            [this] { updateLayerMapping(); });
    connect(m_controller.get(), &DockController::configChanged, this,
            [this] { configureLayerShell(); });
    connect(m_ipcServer.get(), &DockIpcServer::commandReceived, this,
            [this](const DockIpcServer::IpcCommand &command) {
        switch (command.type) {
        case DockIpcServer::Command::Reload:
            m_configWatcher->refresh();
            m_catalog->initialize();
            break;
        case DockIpcServer::Command::Show: m_controller->show(); break;
        case DockIpcServer::Command::Hide: m_controller->hide(); break;
        case DockIpcServer::Command::Quit: m_app.quit(); break;
        case DockIpcServer::Command::Status:
        case DockIpcServer::Command::Unknown:
            break;
        }
    });
    m_controller->attachTyphonConnection(m_typhonConnection.get());
    connect(m_typhonConnection.get(), &TyphonToplevelConnection::diagnostic, this,
            [](const QString &message) {
        qWarning("Dock Typhon toplevel connection: %s", qPrintable(message));
    });
}

void DockApplication::configureLayerShell()
{
    if (!m_window)
        return;
    if (!m_layerState.compiled) {
        m_layerState.configured = false;
        m_layerState.error = QStringLiteral("LayerShellQt not available, using normal window");
        qWarning("%s", qPrintable(m_layerState.error));
        return;
    }

    QString error;
    const bool configured = DockLayerShellSurface::configure(
        m_window, m_configWatcher->config(), qMax(0, m_window->height()),
        QGuiApplication::primaryScreen(), &error);
    m_layerState.configured = configured;
    m_layerState.error = error;
    if (!configured) {
        qCritical("LayerShellQt configuration failed: %s", qPrintable(error));
        return;
    }
    m_layerState.exclusiveZone = qMax(0, m_window->height());
    m_layerState.screen = m_window->screen() ? m_window->screen()->name() : QString();
    updateLayerMapping();
}

void DockApplication::updateLayerMapping()
{
    if (!m_window)
        return;
    const bool shouldMap = m_controller->visible();
    if (m_layerState.configured) {
        QString error;
        if (!shouldMap) {
            DockLayerShellSurface::setMapped(m_window, false, &error);
            m_layerState.mapped = false;
            m_layerState.exclusiveZone = 0;
        } else {
            DockLayerShellSurface::updateExclusiveZone(m_window, m_window->height(), &error);
            DockLayerShellSurface::setMapped(m_window, true, &error);
            m_layerState.mapped = true;
            m_layerState.exclusiveZone = qMax(0, m_window->height());
        }
        if (!error.isEmpty())
            m_layerState.error = error;
    } else {
        m_window->setVisible(shouldMap);
        m_layerState.mapped = shouldMap;
    }
}

QString DockApplication::buildStatusJson() const
{
    QJsonObject layerShell{
        {QStringLiteral("compiled"), m_layerState.compiled},
        {QStringLiteral("configured"), m_layerState.configured},
        {QStringLiteral("mapped"), m_layerState.mapped},
        {QStringLiteral("namespace"), m_layerState.namespaceName},
        {QStringLiteral("exclusiveZone"), m_layerState.exclusiveZone},
        {QStringLiteral("bottomMargin"), m_controller->bottomMargin()},
        {QStringLiteral("screen"), m_layerState.screen}
    };
    QJsonObject windowIntegration{
        {QStringLiteral("available"), false},
        {QStringLiteral("reason"), QStringLiteral("Typhon does not expose a public window-management protocol")}
    };
    QJsonObject config{
        {QStringLiteral("path"), m_configWatcher->configPath()},
        {QStringLiteral("revision"), static_cast<qint64>(m_configWatcher->revision())}
    };
    QJsonObject typhon{
        {QStringLiteral("state"), typhonStateName(m_typhonConnection->state())},
        {QStringLiteral("runtimeKnown"), m_controller->runtimeKnown()},
        {QStringLiteral("generation"), static_cast<qint64>(m_typhonConnection->connectionGeneration())},
        {QStringLiteral("snapshotRevision"), m_typhonConnection->hasSnapshot()
            ? static_cast<qint64>(m_typhonConnection->snapshot().revision) : 0}
    };
    QJsonObject root{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("running"), true},
        {QStringLiteral("visible"), m_controller->visible()},
        {QStringLiteral("enabled"), m_controller->enabled()},
        {QStringLiteral("pinCount"), m_controller->pinCount()},
        {QStringLiteral("resolvedPinCount"), m_controller->resolvedPinCount()},
        {QStringLiteral("launchingCount"), m_controller->launchingCount()},
        {QStringLiteral("config"), config},
        {QStringLiteral("layerShell"), layerShell},
        {QStringLiteral("windowIntegration"), windowIntegration},
        {QStringLiteral("typhon"), typhon},
        {QStringLiteral("lastError"), !m_controller->lastError().isEmpty()
            ? m_controller->lastError()
            : (!m_configWatcher->lastError().isEmpty()
                   ? m_configWatcher->lastError() : m_layerState.error)}
    };
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}
