#include "platform/wayland/BarSurfaceManager.hpp"

#include "core/BarController.hpp"
#include "core/BarPopupController.hpp"
#include "../../../Shell/core/ContextMenuController.hpp"
#include "platform/wayland/BarSurfaceBundle.hpp"

#include <QGuiApplication>
#include <QDebug>
#include <QQmlApplicationEngine>
#include <QScreen>

#include <utility>

BarSurfaceManager::BarSurfaceManager(QGuiApplication &application, QQmlApplicationEngine &engine,
                                     BarController *barController,
                                     BarClockService *clockService,
                                     WorkspaceModel *workspaceModel,
                                     Astrea::System::AudioService *audioService,
                                     Astrea::System::NetworkService *networkService,
                                     Astrea::System::BluetoothService *bluetoothService,
                                     QObject *parent,
                                     BundleFactory bundleFactory,
                                     Astrea::StatusNotifier::StatusNotifierService *statusNotifier,
                                     Astrea::Shell::ContextMenuController *contextMenuController)
    : QObject(parent)
    , m_application(application)
    , m_engine(engine)
    , m_barController(barController)
    , m_clockService(clockService)
    , m_workspaceModel(workspaceModel)
    , m_audioService(audioService)
    , m_networkService(networkService)
    , m_bluetoothService(bluetoothService)
    , m_statusNotifier(statusNotifier)
    , m_contextMenuController(contextMenuController)
    , m_bundleFactory(std::move(bundleFactory))
{
    if (!m_bundleFactory) {
        m_bundleFactory = [this](QScreen *screen, QObject *parent) {
            return new BarSurfaceBundle(screen, &m_engine, m_barController, m_clockService,
                                        m_workspaceModel, m_audioService, m_networkService,
                                        m_bluetoothService, parent, m_statusNotifier,
                                        m_contextMenuController);
        };
    }
    connect(&m_application, &QGuiApplication::screenAdded,
            this, [this](QScreen *screen) {
        if (m_stopped || !m_initialized)
            return;
        QString error;
        if (!addScreen(screen, &error)) {
            qCritical("Astrea shell Bar output initialization failed: %s", qPrintable(error));
            m_application.exit(1);
        }
    });
    connect(&m_application, &QGuiApplication::screenRemoved,
            this, [this](QScreen *screen) {
        if (!m_stopped && m_initialized)
            removeScreen(screen);
    });
    if (m_barController) {
        connect(m_barController, &BarController::enabledChanged,
                this, &BarSurfaceManager::syncBarEnablement);
    }
}

BarSurfaceManager::~BarSurfaceManager()
{
    shutdown();
}

bool BarSurfaceManager::initialize(QString *errorOut)
{
    if (m_stopped) {
        if (errorOut)
            *errorOut = QStringLiteral("Bar surface manager has been shut down");
        return false;
    }
    if (m_initialized)
        return true;
    m_initialized = true;
    for (QScreen *screen : m_application.screens()) {
        if (!addScreen(screen, errorOut)) {
            shutdown();
            return false;
        }
    }
    return true;
}

void BarSurfaceManager::shutdown()
{
    if (m_stopped)
        return;
    m_stopped = true;
    m_initialized = false;
    QObject::disconnect(&m_application, nullptr, this, nullptr);
    if (m_barController)
        QObject::disconnect(m_barController, nullptr, this, nullptr);

    const auto bundles = m_bundles;
    for (QScreen *screen : bundles.keys())
        QObject::disconnect(screen, &QScreen::geometryChanged, this, nullptr);
    m_bundles.clear();
    for (BarSurfaceBundle *bundle : bundles)
        delete bundle;
    if (!bundles.isEmpty()) {
        emit bundleCountChanged();
        emit popupStateChanged();
        emit layerStateChanged();
    }
}

void BarSurfaceManager::closePopups()
{
    bool changed = false;
    for (BarSurfaceBundle *bundle : m_bundles) {
        if (!bundle || !bundle->popupController() || !bundle->popupController()->isOpen())
            continue;
        bundle->popupController()->close();
        bundle->popupController()->completeClose();
        changed = true;
    }
    if (changed)
        emit popupStateChanged();
}

bool BarSurfaceManager::popupOpen() const
{
    for (const BarSurfaceBundle *bundle : m_bundles) {
        if (bundle && bundle->popupOpen())
            return true;
    }
    return false;
}

bool BarSurfaceManager::layerConfigurationRequested() const
{
    if (m_bundles.isEmpty())
        return false;
    for (const BarSurfaceBundle *bundle : m_bundles) {
        if (!bundle || !bundle->layerConfigurationRequested())
            return false;
    }
    return true;
}

bool BarSurfaceManager::addScreen(QScreen *screen, QString *errorOut)
{
    if (m_stopped || !m_initialized)
        return false;
    if (!screen || m_bundles.contains(screen))
        return true;
    auto *bundle = m_bundleFactory(screen, this);
    if (!bundle) {
        if (errorOut)
            *errorOut = QStringLiteral("Bar surface bundle factory returned null");
        return false;
    }
    if (!bundle->initialize(errorOut)) {
        delete bundle;
        return false;
    }
    const QPointer<QScreen> trackedScreen(screen);
    connect(screen, &QScreen::geometryChanged, this,
            [this, trackedScreen](const QRect &) {
        handleScreenGeometryChanged(trackedScreen.data());
    });
    connect(bundle, &BarSurfaceBundle::popupStateChanged,
            this, &BarSurfaceManager::popupStateChanged);
    m_bundles.insert(screen, bundle);
    bundle->setBarEnabled(!m_barController || m_barController->enabled());
    bundle->map();
    emit bundleCountChanged();
    emit popupStateChanged();
    emit layerStateChanged();
    return true;
}

void BarSurfaceManager::removeScreen(QScreen *screen)
{
    if (m_stopped || !m_initialized)
        return;
    auto it = m_bundles.find(screen);
    if (it == m_bundles.end())
        return;
    QObject::disconnect(screen, &QScreen::geometryChanged, this, nullptr);
    BarSurfaceBundle *bundle = it.value();
    m_bundles.erase(it);
    delete bundle;
    emit bundleCountChanged();
    emit popupStateChanged();
    emit layerStateChanged();
}

void BarSurfaceManager::handleScreenGeometryChanged(QScreen *screen)
{
    if (m_stopped || !m_initialized)
        return;
    if (auto it = m_bundles.constFind(screen); it != m_bundles.constEnd() && it.value())
        it.value()->updateForScreen();
}

void BarSurfaceManager::syncBarEnablement()
{
    if (m_stopped || !m_initialized)
        return;
    const bool enabled = !m_barController || m_barController->enabled();
    for (BarSurfaceBundle *bundle : m_bundles) {
        if (bundle)
            bundle->setBarEnabled(enabled);
    }
    emit popupStateChanged();
    emit layerStateChanged();
}
