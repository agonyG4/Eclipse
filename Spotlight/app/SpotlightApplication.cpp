#include "app/SpotlightApplication.hpp"

#include "app/CommandLine.hpp"
#include "core/SpotlightController.hpp"
#include "icons/AstreaIconProvider.hpp"
#include "icons/AstreaIconTheme.hpp"
#include "platform/ipc/SpotlightIpcServer.hpp"
#include "platform/runtime/SpotlightRuntimePaths.hpp"
#include "platform/wayland/LayerShellSurface.hpp"
#include "services/AstreaI18n.hpp"
#include "services/GameModeMonitor.hpp"
#include "services/SpotlightConfigWatcher.hpp"

#include <QDir>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QTextStream>
#include <QTimer>

static constexpr auto kIpcName = "astrea-spotlight-v3";

SpotlightApplication::SpotlightApplication(QGuiApplication &app)
    : QObject(&app)
    , m_app(app)
{
}

int SpotlightApplication::run(int argc, char **argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    m_app.setApplicationName(QStringLiteral("Astrea Spotlight"));
    m_app.setOrganizationName(QStringLiteral("AstreaOS"));
    m_app.setQuitOnLastWindowClosed(false);

    m_request = CommandLine::parse(m_app.arguments().mid(1));
    AstreaIconTheme::apply();

    if (m_request.mode == CommandLineRequest::Mode::IconThemeDiagnostics) {
        const auto resolved = AstreaIconTheme::resolveWithSource();
        QJsonArray pathArr;
        for (const auto &p : AstreaIconTheme::searchPaths())
            pathArr.append(p);
        QJsonObject obj;
        obj[QStringLiteral("selected")] = resolved.theme;
        obj[QStringLiteral("source")] = resolved.source;
        obj[QStringLiteral("qtTheme")] = QIcon::themeName();
        obj[QStringLiteral("fallback")] = QIcon::fallbackThemeName();
        obj[QStringLiteral("searchPaths")] = pathArr;
        QTextStream out(stdout);
        out << QJsonDocument(obj).toJson(QJsonDocument::Indented) << '\n';
        return 0;
    }

    if (m_request.mode == CommandLineRequest::Mode::ResolveIcon) {
        AstreaIconProvider provider;
        QSize size;
        const QPixmap pm = provider.requestPixmap(m_request.argument, &size, QSize(48, 48));
        QJsonObject result;
        result.insert(QStringLiteral("iconName"), m_request.argument);
        result.insert(QStringLiteral("selectedTheme"), QIcon::themeName());
        result.insert(QStringLiteral("fallbackTheme"), QIcon::fallbackThemeName());
        result.insert(QStringLiteral("resolved"), !pm.isNull());
        result.insert(QStringLiteral("requestedSize"), QStringLiteral("48x48"));
        if (!pm.isNull()) {
            result.insert(QStringLiteral("size"),
                          QStringLiteral("%1x%2").arg(size.width()).arg(size.height()));
        }
        QTextStream out(stdout);
        out << QJsonDocument(result).toJson(QJsonDocument::Indented) << '\n';
        return pm.isNull() ? 1 : 0;
    }

    const int clientResult = runClientCommand(m_request);
    if (clientResult >= 0)
        return clientResult;

    if (!initializeRuntime())
        return 1;
    if (!initializeBackend())
        return 1;
    if (!initializeServices())
        return 1;

    connectSignals();

    // Sync initial watcher state to controller (signals emitted during
    // construction were lost because connections didn't exist yet).
    m_controller->setComponentEnabled(m_configWatcher->componentEnabled());
    m_controller->applyConfig(m_configWatcher->spotlightConfig());

    if (!initializeQml())
        return 1;

    return m_app.exec();
}

int SpotlightApplication::runClientCommand(const CommandLineRequest &request)
{
    if (request.daemonMode)
        return -1;

    SpotlightIpcServer::IpcCommand cmd;
    if (request.mode == CommandLineRequest::Mode::Status) {
        cmd.type = SpotlightIpcServer::Command::Status;
        const QByteArray reply = SpotlightIpcServer::requestReply(QLatin1StringView(kIpcName), cmd, 500);
        if (!reply.isEmpty()) {
            QTextStream out(stdout);
            out << QString::fromUtf8(reply).trimmed() << '\n';
            return 0;
        }
        QTextStream out(stdout);
        out << "{\"running\":false}\n";
        return 1;
    }

    if (request.command == QStringLiteral("toggle"))
        cmd.type = SpotlightIpcServer::Command::Toggle;
    else if (request.command == QStringLiteral("hide"))
        cmd.type = SpotlightIpcServer::Command::Hide;
    else if (request.command == QStringLiteral("activate"))
        cmd.type = SpotlightIpcServer::Command::Activate;
    else if (request.command == QStringLiteral("reload-index"))
        cmd.type = SpotlightIpcServer::Command::ReloadIndex;
    else if (request.command == QStringLiteral("query"))
        cmd.type = SpotlightIpcServer::Command::Query;
    else
        cmd.type = SpotlightIpcServer::Command::Show;
    cmd.text = request.argument;

    return SpotlightIpcServer::sendCommand(QLatin1StringView(kIpcName), cmd) ? 0 : -1;
}

bool SpotlightApplication::initializeRuntime()
{
    m_paths = std::make_unique<SpotlightRuntimePaths>(SpotlightRuntimePaths::fromEnvironment());
    m_i18n = std::make_unique<AstreaI18n>(m_paths->i18nDir());
    return true;
}

bool SpotlightApplication::initializeBackend()
{
    m_controller = std::make_unique<SpotlightController>(*m_paths);
    QString error;
    if (!m_controller->init(m_i18n ? m_i18n->language() : QStringLiteral("en_US"), &error)) {
        qCritical("Spotlight backend init failed: %s", qPrintable(error));
        return false;
    }
    m_controller->ensureConfig();
    return true;
}

bool SpotlightApplication::initializeServices()
{
    m_configWatcher = std::make_unique<SpotlightConfigWatcher>(m_paths->configPath(), m_paths->componentsConfigPath());
    m_gameMode = std::make_unique<GameModeMonitor>();
    m_ipcServer = std::make_unique<SpotlightIpcServer>();
    m_appWatcher = std::make_unique<QFileSystemWatcher>();
    m_appRefreshDebounce = std::make_unique<QTimer>();
    m_appRefreshDebounce->setSingleShot(true);
    m_appRefreshDebounce->setInterval(2000);

    updateApplicationDirectoryWatchers();

    if (!m_ipcServer->listen(QLatin1StringView(kIpcName), nullptr)) {
        QLocalSocket probe;
        probe.connectToServer(QLatin1StringView(kIpcName));
        if (probe.waitForConnected(100)) {
            probe.disconnectFromServer();
            qCritical("Spotlight IPC server already running at %s", kIpcName);
            return false;
        }
        QLocalServer::removeServer(QLatin1StringView(kIpcName));
        QString error;
        if (!m_ipcServer->listen(QLatin1StringView(kIpcName), &error)) {
            qCritical("Spotlight IPC listen failed: %s", qPrintable(error));
            return false;
        }
    }

    m_ipcServer->setReplyCallback([this](const SpotlightIpcServer::IpcCommand &) -> QString {
        return buildStatusJson();
    });

    return true;
}

bool SpotlightApplication::initializeQml()
{
    m_engine = std::make_unique<QQmlApplicationEngine>();
    m_iconProvider = new AstreaIconProvider;
    m_engine->addImportPath(QStringLiteral("qrc:/"));
    m_engine->rootContext()->setContextProperty(QStringLiteral("AstreaIconProvider"), m_iconProvider);
    m_engine->addImageProvider(QStringLiteral("astrea-icon"), m_iconProvider);
    m_engine->rootContext()->setContextProperty(QStringLiteral("SpotlightController"), m_controller.get());
    m_engine->rootContext()->setContextProperty(QStringLiteral("AstreaI18n"), m_i18n.get());
    m_engine->rootContext()->setContextProperty(QStringLiteral("ConfigWatcher"), m_configWatcher.get());
    m_engine->rootContext()->setContextProperty(QStringLiteral("GameModeMonitor"), m_gameMode.get());

    m_engine->loadFromModule(QStringLiteral("Astrea.Spotlight"), QStringLiteral("Main"));
    if (m_engine->rootObjects().isEmpty()) {
        qCritical("Spotlight QML root failed to load");
        return false;
    }

    // Set LayerShellQt compile-time support flag
#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
    m_layerState.compiled = true;
#else
    m_layerState.compiled = false;
#endif

    auto *window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().constFirst());
    if (window) {
        QString layerError;
        if (LayerShellSurface::configure(window, &layerError)) {
            m_layerState.configured = true;
        } else {
            m_layerState.error = layerError;
#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
            qCritical("LayerShellQt available but configure failed: %s", qPrintable(layerError));
            return false;
#else
            qWarning("LayerShell not available: %s — running as normal window", qPrintable(layerError));
#endif
        }
    }

    m_gameMode->start(10000);
    return true;
}

void SpotlightApplication::connectSignals()
{
    QObject::connect(m_gameMode.get(), &GameModeMonitor::gameModeChanged, this, [this] {
        m_controller->setGameModeActive(m_gameMode->gameModeActive());
    });

    QObject::connect(m_configWatcher.get(), &SpotlightConfigWatcher::componentToggled, this,
                     [this](bool enabled) {
        m_controller->setComponentEnabled(enabled);
    });
    QObject::connect(m_configWatcher.get(), &SpotlightConfigWatcher::configChanged, this, [this] {
        m_controller->applyConfig(m_configWatcher->spotlightConfig());
    });

    QObject::connect(m_appRefreshDebounce.get(), &QTimer::timeout, this, [this] {
        m_controller->reloadIndex();
    });
    QObject::connect(m_appWatcher.get(), &QFileSystemWatcher::directoryChanged, this, [this] {
        m_appRefreshDebounce->start();
    });

    QObject::connect(m_ipcServer.get(), &SpotlightIpcServer::commandReceived,
                     this, [this](const SpotlightIpcServer::IpcCommand &cmd) {
        switch (cmd.type) {
        case SpotlightIpcServer::Command::Show:
            if (m_configWatcher->componentEnabled()) m_controller->show();
            break;
        case SpotlightIpcServer::Command::Hide:
            m_controller->close();
            break;
        case SpotlightIpcServer::Command::Toggle:
            if (m_configWatcher->componentEnabled()) m_controller->toggle();
            break;
        case SpotlightIpcServer::Command::Query:
            if (m_configWatcher->componentEnabled()) m_controller->setQuery(cmd.text);
            break;
        case SpotlightIpcServer::Command::Activate:
            if (m_configWatcher->componentEnabled()) m_controller->activateCurrent();
            break;
        case SpotlightIpcServer::Command::ReloadIndex:
            m_controller->close();
            m_controller->reloadIndex();
            break;
        case SpotlightIpcServer::Command::Status:
        case SpotlightIpcServer::Command::Unknown:
            break;
        }
    });

    QObject::connect(m_i18n.get(), &AstreaI18n::languageChanged, this, [this] {
        if (m_controller)
            m_controller->setLocale(m_i18n->language());
    });
}

void SpotlightApplication::updateApplicationDirectoryWatchers()
{
    if (!m_appWatcher)
        return;

    for (const auto &p : m_appWatcher->files())
        m_appWatcher->removePath(p);
    for (const auto &p : m_appWatcher->directories())
        m_appWatcher->removePath(p);

    const QJsonArray backendDirs = m_controller->watchedDirectories();
    if (!backendDirs.isEmpty()) {
        for (const auto &d : backendDirs) {
            const QString dirPath = d.toString();
            if (QDir(dirPath).exists())
                m_appWatcher->addPath(dirPath);
        }
        return;
    }

    QStringList fallback;
    fallback << QDir::homePath() + QStringLiteral("/.local/share/applications")
             << QStringLiteral("/usr/share/applications")
             << QStringLiteral("/usr/local/share/applications")
             << QStringLiteral("/var/lib/flatpak/exports/share/applications")
             << QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share/applications");
    for (const auto &d : fallback) {
        if (QDir(d).exists())
            m_appWatcher->addPath(d);
    }
}

QString SpotlightApplication::buildStatusJson() const
{
    QString escapedError = m_layerState.error;
    escapedError.replace(QLatin1Char('"'), QStringLiteral("'"));
    escapedError.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    return QStringLiteral("{\"running\":true,\"visible\":%1,\"open\":%2,\"results\":%3,"
                          "\"componentEnabled\":%4,\"gameMode\":%5,\"layerShell\":%6,"
                          "\"layerShellCompiled\":%7,\"layerShellConfigured\":%8,"
                          "\"layerShellError\":\"%9\","
                          "\"iconTheme\":\"%10\",\"iconFallbackTheme\":\"%11\"}")
        .arg(m_controller->surfaceVisible() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_controller->isOpen() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_controller->resultCount())
        .arg(m_configWatcher->componentEnabled() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_gameMode->gameModeActive() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_layerState.configured ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_layerState.compiled ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_layerState.configured ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(escapedError)
        .arg(QIcon::themeName())
        .arg(QIcon::fallbackThemeName());
}
