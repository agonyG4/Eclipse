#include "app/AltTabApplication.hpp"

#include "app/CommandLine.hpp"
#include "core/AltTabController.hpp"
#include "core/AltTabShortcutRouter.hpp"
#include "platform/compositor/CompositorBackend.hpp"
#include "platform/compositor/CompositorBackendFactory.hpp"
#include "platform/ipc/AltTabIpcServer.hpp"
#include "platform/runtime/AltTabRuntimePaths.hpp"
#include "platform/wayland/LayerShellSurface.hpp"
#include "services/AppIdentityResolver.hpp"
#include "services/AltTabConfigWatcher.hpp"

#include "icons/AstreaIconProvider.hpp"
#include "icons/AstreaIconTheme.hpp"

#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QTextStream>
#include <QProcessEnvironment>

static constexpr auto kIpcName = "astrea-alt-tab-v1";

AltTabApplication::AltTabApplication(QGuiApplication &app)
    : QObject(&app)
    , m_app(app)
{
}

int AltTabApplication::run(int argc, char **argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    m_app.setApplicationName(QStringLiteral("Astrea Alt+Tab"));
    m_app.setOrganizationName(QStringLiteral("AstreaOS"));
    m_app.setQuitOnLastWindowClosed(false);

    m_request = CommandLine::parse(m_app.arguments().mid(1));

    const auto env = QProcessEnvironment::systemEnvironment();
    if (env.contains(QStringLiteral("ASTREA_COMPOSITOR_BACKEND"))) {
        m_request.backend = env.value(QStringLiteral("ASTREA_COMPOSITOR_BACKEND"));
    }

    AstreaIconTheme::apply();

    const int clientResult = runClientCommand(m_request);
    if (clientResult >= 0)
        return clientResult;

    if (!initializeRuntime())
        return 1;
    if (!initializeServices())
        return 1;

    connectSignals();

    if (!initializeQml())
        return 1;

    m_backend->start();

    return m_app.exec();
}

int AltTabApplication::runClientCommand(const CommandLineRequest &request)
{
    if (request.daemonMode)
        return -1;

    AltTabIpcServer::IpcCommand cmd;

    auto getCommandType = [](CommandLineRequest::Mode mode) -> AltTabIpcServer::Command {
        switch (mode) {
        case CommandLineRequest::Mode::Next: return AltTabIpcServer::Command::Next;
        case CommandLineRequest::Mode::Previous: return AltTabIpcServer::Command::Previous;
        case CommandLineRequest::Mode::Commit: return AltTabIpcServer::Command::Commit;
        case CommandLineRequest::Mode::Cancel: return AltTabIpcServer::Command::Cancel;
        case CommandLineRequest::Mode::Show: return AltTabIpcServer::Command::Show;
        case CommandLineRequest::Mode::Hide: return AltTabIpcServer::Command::Hide;
        case CommandLineRequest::Mode::ReloadWindows: return AltTabIpcServer::Command::ReloadWindows;
        case CommandLineRequest::Mode::Status: return AltTabIpcServer::Command::Status;
        default: return AltTabIpcServer::Command::Unknown;
        }
    };

    if (request.mode == CommandLineRequest::Mode::Status) {
        cmd.type = AltTabIpcServer::Command::Status;
        const QByteArray reply = AltTabIpcServer::requestReply(QLatin1StringView(kIpcName), cmd, 500);
        if (!reply.isEmpty()) {
            QTextStream out(stdout);
            out << QString::fromUtf8(reply).trimmed() << '\n';
            return 0;
        }
        QTextStream out(stdout);
        out << "{\"running\":false}\n";
        return 1;
    }

    cmd.type = getCommandType(request.mode);
    cmd.text = request.argument;

    return AltTabIpcServer::sendCommand(QLatin1StringView(kIpcName), cmd) ? 0 : 1;
}

bool AltTabApplication::initializeRuntime()
{
    m_paths = std::make_unique<AltTabRuntimePaths>(AltTabRuntimePaths::fromEnvironment());
    m_identityResolver = std::make_unique<AppIdentityResolver>();
    m_identityResolver->initialize();
    return true;
}

bool AltTabApplication::initializeServices()
{
    m_backend.reset(CompositorBackendFactory::createBackend(m_request.backend));
    if (!m_backend) {
        qCritical("Failed to create compositor backend. Hyprland is required.");
        return false;
    }

    m_controller = std::make_unique<AltTabController>(m_backend.get(), m_identityResolver.get());

    m_configWatcher = std::make_unique<AltTabConfigWatcher>(
        m_paths->alttabConfigPath(), m_paths->componentsConfigPath());
    m_ipcServer = std::make_unique<AltTabIpcServer>();

    QString error;
    if (!m_ipcServer->listen(QLatin1StringView(kIpcName), &error)) {
        qCritical("AltTab IPC listen failed: %s", qPrintable(error));
        return false;
    }

    m_ipcServer->setReplyCallback([this](const AltTabIpcServer::IpcCommand &) -> QString {
        return buildStatusJson();
    });

    m_shortcutClient = std::make_unique<TyphonShortcutClient>();

    return true;
}

bool AltTabApplication::initializeQml()
{
    m_engine = std::make_unique<QQmlApplicationEngine>();
    m_iconProvider = new AstreaIconProvider;
    m_engine->addImportPath(QStringLiteral("qrc:/"));
    m_engine->rootContext()->setContextProperty(QStringLiteral("AstreaIconProvider"), m_iconProvider);
    m_engine->addImageProvider(QStringLiteral("astrea-icon"), m_iconProvider);
    m_engine->rootContext()->setContextProperty(QStringLiteral("AltTabController"), m_controller.get());
    m_engine->rootContext()->setContextProperty(QStringLiteral("AltTabWindowModel"), m_controller->windowModel());

    m_engine->loadFromModule(QStringLiteral("Astrea.AltTab"), QStringLiteral("Main"));
    if (m_engine->rootObjects().isEmpty()) {
        qCritical("AltTab QML root failed to load");
        return false;
    }

#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
    m_layerState.compiled = true;
#else
    m_layerState.compiled = false;
#endif

    auto *window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().constFirst());
    if (window) {
        m_overlayWindow = window;
        QString layerError;
        if (AltTabLayerShellSurface::configure(window, &layerError)) {
            m_layerState.configured = true;
        } else {
            m_layerState.error = layerError;
#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
            qCritical("LayerShellQt available but configure failed: %s", qPrintable(layerError));
            return false;
#else
            qWarning("LayerShell not available: %s", qPrintable(layerError));
#endif
        }
    }

    return true;
}

void AltTabApplication::connectSignals()
{
    QObject::connect(m_configWatcher.get(), &AltTabConfigWatcher::componentToggled, this,
                     [this](bool enabled) {
        if (enabled) {
            if (m_shortcutClient)
                m_shortcutClient->start();
            return;
        }
        if (m_controller->isOpen())
            m_controller->cancel();
        if (m_shortcutClient)
            m_shortcutClient->stop();
    });

    QObject::connect(m_configWatcher.get(), &AltTabConfigWatcher::configChanged, this, [this] {
    });

    QObject::connect(m_controller.get(), &AltTabController::focusRequested, this, [this] {
        placeOverlayOnFocusedScreen();
    });

    QObject::connect(m_ipcServer.get(), &AltTabIpcServer::commandReceived,
                     this, [this](const AltTabIpcServer::IpcCommand &cmd) {
        if (!m_configWatcher->componentEnabled()) {
            if (cmd.type != AltTabIpcServer::Command::Hide)
                return;
        }
        switch (cmd.type) {
        case AltTabIpcServer::Command::Next:
            m_controller->step(1);
            break;
        case AltTabIpcServer::Command::Previous:
            m_controller->step(-1);
            break;
        case AltTabIpcServer::Command::Commit:
            m_controller->commit();
            break;
        case AltTabIpcServer::Command::Cancel:
            m_controller->cancel();
            break;
        case AltTabIpcServer::Command::Show:
            m_controller->show();
            break;
        case AltTabIpcServer::Command::Hide:
            m_controller->hide();
            break;
        case AltTabIpcServer::Command::ReloadWindows:
            m_controller->reloadWindows();
            break;
        case AltTabIpcServer::Command::Status:
        case AltTabIpcServer::Command::Unknown:
            break;
        }
    });

    QObject::connect(m_shortcutClient.get(), &TyphonShortcutClient::diagnostic, this,
                     [](const QString &message) {
        qWarning("AltTab Typhon shortcut client: %s", qPrintable(message));
    });
    QObject::connect(m_shortcutClient.get(), &TyphonShortcutClient::shortcutEvent, this,
                     [this](const QString &namespaceName, const QString &name,
                            TyphonShortcutPhase phase, std::uint32_t, std::uint32_t) {
        if (!m_configWatcher->componentEnabled())
            return;

        switch (mapTyphonShortcut(namespaceName, name, phase)) {
        case AltTabShortcutAction::Next:
            m_controller->step(1);
            break;
        case AltTabShortcutAction::Previous:
            m_controller->step(-1);
            break;
        case AltTabShortcutAction::Commit:
            m_controller->commit();
            break;
        case AltTabShortcutAction::Ignore:
            break;
        }
    });

    if (m_configWatcher->componentEnabled() && m_shortcutClient)
        m_shortcutClient->start();
}

QString AltTabApplication::buildStatusJson() const
{
    const QJsonObject status = QJsonDocument::fromJson(
        m_controller->buildStatusJson().toUtf8()).object();

    const QString backendName = m_backend ? m_backend->descriptor().name : QStringLiteral("none");
    const BackendState backendState = m_backend ? m_backend->state() : BackendState::Stopped;
    const auto snapshot = m_backend ? m_backend->cachedSnapshot() : std::nullopt;

    QJsonObject backend;
    backend[QStringLiteral("name")] = backendName;
    backend[QStringLiteral("state")] = static_cast<int>(backendState);
    backend[QStringLiteral("stateName")] = [backendState]() -> QString {
        switch (backendState) {
            case BackendState::Stopped: return QStringLiteral("stopped");
            case BackendState::Starting: return QStringLiteral("starting");
            case BackendState::ConnectingEvents: return QStringLiteral("connectingEvents");
            case BackendState::LoadingInitialSnapshot: return QStringLiteral("loadingInitialSnapshot");
            case BackendState::Ready: return QStringLiteral("ready");
            case BackendState::Degraded: return QStringLiteral("degraded");
            case BackendState::Disconnected: return QStringLiteral("disconnected");
            case BackendState::Unsupported: return QStringLiteral("unsupported");
        }
        return QStringLiteral("unknown");
    }();
    backend[QStringLiteral("activeOutput")] = snapshot ? snapshot->activeOutputId.value : QString();
    if (snapshot) {
        backend[QStringLiteral("activeWindowId")] = snapshot->activeWindowId.value;
        if (!snapshot->focusedMonitorName.isEmpty())
            backend[QStringLiteral("focusedMonitor")] = snapshot->focusedMonitorName;
    }
    backend[QStringLiteral("lastError")] = QString();

    QJsonObject overlay;
    overlay[QStringLiteral("compiled")] = m_layerState.compiled;
    overlay[QStringLiteral("configured")] = m_layerState.configured;
    overlay[QStringLiteral("mapped")] = status.value(QStringLiteral("visible")).toBool();
    overlay[QStringLiteral("namespace")] = QStringLiteral("astrea-alt-tab");
    overlay[QStringLiteral("screen")] = m_overlayWindow && m_overlayWindow->screen() ? m_overlayWindow->screen()->name() : QString();
    overlay[QStringLiteral("lastError")] = m_layerState.error;

    QJsonObject identity;
    identity[QStringLiteral("themeRevision")] = m_identityResolver ? m_identityResolver->themeRevision() : 0;

    QJsonObject root;
    root[QStringLiteral("schemaVersion")] = 2;
    root[QStringLiteral("running")] = true;
    root[QStringLiteral("state")] = status.value(QStringLiteral("state")).toString();
    root[QStringLiteral("visible")] = status.value(QStringLiteral("visible")).toBool();
    root[QStringLiteral("windowCount")] = status.value(QStringLiteral("windowCount")).toInt();
    root[QStringLiteral("selectedIndex")] = status.value(QStringLiteral("selectedIndex")).toInt();
    root[QStringLiteral("selectedWindowId")] = status.value(QStringLiteral("selectedWindowId")).toString();
    root[QStringLiteral("backend")] = backend;
    root[QStringLiteral("overlay")] = overlay;
    root[QStringLiteral("identity")] = identity;

    QJsonObject shortcuts;
    if (m_shortcutClient) {
        shortcuts[QStringLiteral("state")] = [this]() -> QString {
            switch (m_shortcutClient->state()) {
            case TyphonShortcutConnectionState::Stopped: return QStringLiteral("stopped");
            case TyphonShortcutConnectionState::Connecting: return QStringLiteral("connecting");
            case TyphonShortcutConnectionState::WaitingForManager: return QStringLiteral("waitingForManager");
            case TyphonShortcutConnectionState::Registering: return QStringLiteral("registering");
            case TyphonShortcutConnectionState::Ready: return QStringLiteral("ready");
            case TyphonShortcutConnectionState::Degraded: return QStringLiteral("degraded");
            case TyphonShortcutConnectionState::Disconnected: return QStringLiteral("disconnected");
            case TyphonShortcutConnectionState::Unsupported: return QStringLiteral("unsupported");
            }
            return QStringLiteral("unknown");
        }();
        shortcuts[QStringLiteral("registeredCount")] = m_shortcutClient->registeredShortcutCount();
        shortcuts[QStringLiteral("generation")] = static_cast<qint64>(m_shortcutClient->connectionGeneration());
    } else {
        shortcuts[QStringLiteral("state")] = QStringLiteral("stopped");
        shortcuts[QStringLiteral("registeredCount")] = 0;
    }
    root[QStringLiteral("shortcuts")] = shortcuts;
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void AltTabApplication::placeOverlayOnFocusedScreen()
{
    if (!m_overlayWindow)
        return;

    QScreen *screen = nullptr;
    const auto snapshot = m_backend ? m_backend->cachedSnapshot() : std::nullopt;
    const QString activeOutput = snapshot ? snapshot->activeOutputId.value : QString();
    const QString focusedMonitor = snapshot ? snapshot->focusedMonitorName : QString();
    const QString targetOutput = !focusedMonitor.isEmpty() ? focusedMonitor : activeOutput;
    if (!targetOutput.isEmpty()) {
        const auto screens = QGuiApplication::screens();
        for (QScreen *candidate : screens) {
            if (candidate && candidate->name() == targetOutput) {
                screen = candidate;
                break;
            }
        }
    }

    if (!screen)
        screen = m_overlayWindow->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    m_overlayWindow->setScreen(screen);
    m_overlayWindow->setGeometry(screen->geometry());
}
