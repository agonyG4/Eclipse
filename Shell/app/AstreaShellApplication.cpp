#include "app/AstreaShellApplication.hpp"

#include "AltTab/core/AltTabController.hpp"
#include "AltTab/platform/wayland/LayerShellSurface.hpp"
#include "Dock/core/DockController.hpp"
#include "Dock/platform/wayland/DockLayerShellSurface.hpp"
#include "Spotlight/core/SpotlightController.hpp"
#include "Spotlight/platform/wayland/LayerShellSurface.hpp"
#include "Spotlight/services/AstreaI18n.hpp"
#include "platform/ipc/ShellIpcServer.hpp"
#include "runtime/ShellRuntime.hpp"
#include "platform/wayland/LayerShellHelper.hpp"
#include "icons/AstreaIconProvider.hpp"
#include "icons/AstreaIconTheme.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QUrl>

namespace {

constexpr QLatin1StringView kIpcName("astrea-shell-v1");

bool isWaylandPlatform(const QGuiApplication &application)
{
    return application.platformName().startsWith(QStringLiteral("wayland"), Qt::CaseInsensitive);
}

} // namespace

AstreaShellApplication::AstreaShellApplication(QGuiApplication &application)
    : QObject(&application), m_application(application)
{
}

AstreaShellApplication::~AstreaShellApplication() = default;

int AstreaShellApplication::run()
{
    m_application.setApplicationName(QStringLiteral("Astrea Shell"));
    m_application.setOrganizationName(QStringLiteral("AstreaOS"));
    m_application.setQuitOnLastWindowClosed(false);
    AstreaIconTheme::apply();

    if (!AstreaLayerShellHelper::compiled()) {
        qCritical("Astrea shell requires a production LayerShellQt build; configure with "
                  "-DASTREA_ENABLE_LAYER_SHELL=ON");
        return 1;
    }
    if (!isWaylandPlatform(m_application)) {
        qCritical("Astrea shell requires the Qt Wayland platform; current platform is %s",
                  qPrintable(m_application.platformName()));
        return 1;
    }

    if (!initializeRuntime() || !listenForCommands() || !initializeQml())
        return 1;

    m_runtime->start();
    return m_application.exec();
}

bool AstreaShellApplication::initializeRuntime()
{
    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QString backend = environment.value(QStringLiteral("ASTREA_COMPOSITOR_BACKEND"),
                                               QStringLiteral("auto"));

    m_runtime = std::make_unique<ShellRuntime>();
    QString error;
    if (!m_runtime->initialize(backend, &error)) {
        qCritical("Astrea shell runtime initialization failed: %s", qPrintable(error));
        return false;
    }

    const SpotlightRuntimePaths paths = SpotlightRuntimePaths::fromEnvironment();
    m_i18n = std::make_unique<AstreaI18n>(paths.i18nDir());
    if (m_i18n->language() != QStringLiteral("en_US")) {
        if (!m_runtime->spotlightController()->setLocale(m_i18n->language(), &error)) {
            qCritical("Astrea shell locale initialization failed: %s", qPrintable(error));
            return false;
        }
    }
    return true;
}

bool AstreaShellApplication::listenForCommands()
{
    QString error;
    if (!m_runtime->ipcServer()->listen(QString::fromLatin1(kIpcName), &error)) {
        qCritical("Astrea shell IPC listen failed: %s", qPrintable(error));
        return false;
    }
    m_runtime->ipcServer()->setReplyCallback(
        [this](const ShellIpcServer::Command &) { return statusJson(); });
    connect(m_runtime->ipcServer(), &ShellIpcServer::commandReceived,
            this, &AstreaShellApplication::handleCommand);
    return true;
}

bool AstreaShellApplication::loadSurface(const QUrl &url, QQuickWindow **windowOut)
{
    const int rootCount = m_engine->rootObjects().size();
    m_engine->load(url);
    if (m_engine->rootObjects().size() <= rootCount) {
        qWarning("Astrea shell QML surface failed to load: %s", qPrintable(url.toString()));
        return false;
    }

    auto *window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().constLast());
    if (!window) {
        qWarning("Astrea shell QML surface is not a window: %s", qPrintable(url.toString()));
        return false;
    }
    if (windowOut)
        *windowOut = window;
    return true;
}

bool AstreaShellApplication::initializeQml()
{
    m_engine = std::make_unique<QQmlApplicationEngine>();
    connect(m_engine.get(), &QQmlApplicationEngine::warnings, this,
            [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            qWarning("Astrea shell QML: %s", qPrintable(warning.toString()));
    });

    auto *context = m_engine->rootContext();
    auto *iconProvider = new AstreaIconProvider;
    context->setContextProperty(QStringLiteral("AstreaIconProvider"), iconProvider);
    m_engine->addImageProvider(QStringLiteral("astrea-icon"), iconProvider);
    context->setContextProperty(QStringLiteral("DockController"), m_runtime->dockController());
    context->setContextProperty(QStringLiteral("AltTabController"),
                                m_runtime->altTabController());
    context->setContextProperty(QStringLiteral("AltTabWindowModel"),
                                m_runtime->altTabController()->windowModel());
    context->setContextProperty(QStringLiteral("SpotlightController"),
                                m_runtime->spotlightController());
    context->setContextProperty(QStringLiteral("AstreaI18n"), m_i18n.get());

    QQuickWindow *window = nullptr;
    if (!loadSurface(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Dock/qml/Main.qml")),
                     &window))
        return false;
    m_dockWindow = window;

    if (!loadSurface(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/AltTab/qml/Main.qml")),
                     &window)) {
        qCritical("Astrea shell Alt+Tab surface failed to load");
        return false;
    } else {
        m_altTabWindow = window;
    }

    if (!loadSurface(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Spotlight/qml/Main.qml")),
                     &window)) {
        qCritical("Astrea shell Spotlight surface failed to load");
        return false;
    } else {
        m_spotlightWindow = window;
    }

    if (!configureSurfaces())
        return false;

    connect(m_runtime->dockController(), &DockController::visibleChanged,
            this, &AstreaShellApplication::syncDockVisibility);
    connect(m_runtime->dockController(), &DockController::configChanged,
            this, &AstreaShellApplication::configureDockSurface);
    syncDockVisibility();
    return true;
}

bool AstreaShellApplication::configureSurfaces()
{
    if (!m_dockWindow) {
        qCritical("Astrea shell Dock surface is unavailable");
        return false;
    }
    if (!DockLayerShellSurface::compiled()) {
        qCritical("Astrea shell Dock requires the compiled LayerShellQt integration");
        return false;
    }
    if (!configureDockSurface()) {
        qCritical("Astrea shell Dock Layer Shell setup failed");
        return false;
    }

    if (!m_altTabWindow) {
        qCritical("Astrea shell Alt+Tab surface is unavailable");
        return false;
    }
    QString error;
    if (!AltTabLayerShellSurface::configure(m_altTabWindow, &error)) {
        qCritical("Astrea shell Alt+Tab Layer Shell setup failed: %s", qPrintable(error));
        return false;
    }
    m_altTabLayerConfigured = true;

    if (!m_spotlightWindow) {
        qCritical("Astrea shell Spotlight surface is unavailable");
        return false;
    }
    error.clear();
    if (!LayerShellSurface::configure(m_spotlightWindow, &error)) {
        qCritical("Astrea shell Spotlight Layer Shell setup failed: %s", qPrintable(error));
        return false;
    }
    m_spotlightLayerConfigured = true;
    return true;
}

bool AstreaShellApplication::configureDockSurface()
{
    if (!m_dockWindow || !m_runtime || !m_runtime->dockConfig())
        return false;
    QString error;
    const bool configured = DockLayerShellSurface::configure(
        m_dockWindow, m_runtime->dockConfig()->config(), qMax(0, m_dockWindow->height()),
        QGuiApplication::primaryScreen(), &error);
    if (!configured) {
        qCritical("Astrea shell Dock Layer Shell setup failed: %s", qPrintable(error));
        m_application.exit(1);
    } else {
        m_dockLayerConfigured = true;
    }
    return configured;
}

void AstreaShellApplication::syncDockVisibility()
{
    if (!m_dockWindow)
        return;
    const bool visible = m_runtime->dockController()->visible();
    if (!m_dockLayerConfigured) {
        qCritical("Astrea shell Dock cannot change visibility before Layer Shell setup");
        m_application.exit(1);
        return;
    }
    QString error;
    if (!DockLayerShellSurface::setMapped(m_dockWindow, visible, &error)) {
        qCritical("Astrea shell Dock Layer Shell mapping failed: %s", qPrintable(error));
        m_application.exit(1);
    }
}

void AstreaShellApplication::handleCommand(const ShellIpcServer::Command &command)
{
    if (!command.valid)
        return;

    if (command.feature == QStringLiteral("shell")) {
        if (command.action == QStringLiteral("quit"))
            m_application.quit();
        else if (command.action == QStringLiteral("reload")) {
            m_runtime->reloadCatalog();
            m_runtime->reloadDockConfig();
            m_runtime->reloadAltTabConfig();
            m_runtime->reloadSpotlightConfig();
        }
        return;
    }

    if (command.feature == QStringLiteral("dock")) {
        if (command.action == QStringLiteral("reload")) {
            m_runtime->reloadDockConfig();
            m_runtime->reloadCatalog();
        } else if (command.action == QStringLiteral("show")) {
            m_runtime->dockController()->show();
        } else if (command.action == QStringLiteral("hide")) {
            m_runtime->dockController()->hide();
        } else if (command.action == QStringLiteral("quit")) {
            m_application.quit();
        }
        return;
    }

    if (command.feature == QStringLiteral("alttab")) {
        if (command.action == QStringLiteral("next"))
            m_runtime->altTabController()->step(1);
        else if (command.action == QStringLiteral("previous"))
            m_runtime->altTabController()->step(-1);
        else if (command.action == QStringLiteral("commit"))
            m_runtime->altTabController()->commit();
        else if (command.action == QStringLiteral("cancel")
                 || command.action == QStringLiteral("hide"))
            m_runtime->altTabController()->cancel();
        else if (command.action == QStringLiteral("show"))
            m_runtime->altTabController()->show();
        else if (command.action == QStringLiteral("reload"))
            m_runtime->reloadAltTabConfig();
        return;
    }

    if (command.feature == QStringLiteral("spotlight")) {
        if (command.action == QStringLiteral("show"))
            m_runtime->spotlightController()->show();
        else if (command.action == QStringLiteral("hide"))
            m_runtime->spotlightController()->close();
        else if (command.action == QStringLiteral("toggle"))
            m_runtime->spotlightController()->toggle();
        else if (command.action == QStringLiteral("query")) {
            m_runtime->spotlightController()->show();
            m_runtime->spotlightController()->setQuery(command.argument);
        } else if (command.action == QStringLiteral("activate")) {
            m_runtime->spotlightController()->activateCurrent();
        } else if (command.action == QStringLiteral("reload")) {
            m_runtime->reloadSpotlightConfig();
            m_runtime->spotlightController()->reloadIndex();
            m_runtime->reloadCatalog();
        }
    }
}

QString AstreaShellApplication::statusJson() const
{
    const auto *runtime = m_runtime.get();
    const auto *dock = runtime->dockController();
    const auto *altTab = runtime->altTabController();
    const auto *spotlight = runtime->spotlightController();
    const QJsonObject object{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("running"), true},
        {QStringLiteral("catalogRevision"), runtime->catalog()->revision()},
        {QStringLiteral("layerShell"), QJsonObject{
            {QStringLiteral("compiled"), AstreaLayerShellHelper::compiled()},
            {QStringLiteral("required"), true},
            {QStringLiteral("platform"), m_application.platformName()},
            {QStringLiteral("dockConfigured"), m_dockLayerConfigured},
            {QStringLiteral("altTabConfigured"), m_altTabLayerConfigured},
            {QStringLiteral("spotlightConfigured"), m_spotlightLayerConfigured}}},
        {QStringLiteral("dock"), QJsonObject{
            {QStringLiteral("visible"), dock->visible()},
            {QStringLiteral("enabled"), dock->enabled()},
            {QStringLiteral("pins"), dock->pinCount()}}},
        {QStringLiteral("alttab"), QJsonObject{
            {QStringLiteral("open"), altTab->isOpen()},
            {QStringLiteral("windows"), altTab->windowCount()}}},
        {QStringLiteral("spotlight"), QJsonObject{
            {QStringLiteral("open"), spotlight->isOpen()},
            {QStringLiteral("results"), spotlight->resultCount()}}}
    };
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}
