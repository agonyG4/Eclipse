#include "ContextMenuSurfaceManager.hpp"

#include "core/ContextMenuController.hpp"
#include "ContextMenuSurfaceBundle.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QScreen>

#include <utility>

ContextMenuSurfaceManager::ContextMenuSurfaceManager(QGuiApplication &application,
                                                    QQmlApplicationEngine &engine,
                                                    Astrea::Shell::ContextMenuController *controller,
                                                    QObject *parent,
                                                    BundleFactory bundleFactory,
                                                    Astrea::StatusNotifier::StatusNotifierService *statusNotifier)
    : QObject(parent)
    , m_application(application)
    , m_engine(engine)
    , m_controller(controller)
    , m_statusNotifier(statusNotifier)
    , m_bundleFactory(std::move(bundleFactory))
{
    if (!m_bundleFactory) {
        m_bundleFactory = [this](QScreen *screen, QObject *parentObject) {
            return new ContextMenuSurfaceBundle(screen, &m_engine, m_controller, parentObject,
                                                m_statusNotifier);
        };
    }
    connect(&m_application, &QGuiApplication::screenAdded, this, [this](QScreen *screen) {
        if (m_stopped || !m_initialized)
            return;
        QString error;
        if (!addScreen(screen, &error))
            m_application.exit(1);
    });
    connect(&m_application, &QGuiApplication::screenRemoved, this, [this](QScreen *screen) {
        if (!m_stopped && m_initialized)
            removeScreen(screen);
    });
}

ContextMenuSurfaceManager::~ContextMenuSurfaceManager()
{
    shutdown();
}

bool ContextMenuSurfaceManager::initialize(QString *errorOut)
{
    if (m_stopped) {
        if (errorOut)
            *errorOut = QStringLiteral("Context menu surface manager has been shut down");
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

void ContextMenuSurfaceManager::shutdown()
{
    if (m_stopped)
        return;
    m_stopped = true;
    m_initialized = false;
    QObject::disconnect(&m_application, nullptr, this, nullptr);
    if (m_controller)
        m_controller->shutdown();
    const auto bundles = m_bundles;
    for (QScreen *screen : bundles.keys())
        QObject::disconnect(screen, &QScreen::geometryChanged, this, nullptr);
    m_bundles.clear();
    for (ContextMenuSurfaceBundle *bundle : bundles)
        delete bundle;
    syncMappingNotification();
    if (!bundles.isEmpty()) {
        emit bundleCountChanged();
        emit layerStateChanged();
    }
}

bool ContextMenuSurfaceManager::overlayMapped() const
{
    for (const ContextMenuSurfaceBundle *bundle : m_bundles) {
        if (bundle && bundle->overlayMapped())
            return true;
    }
    return false;
}

bool ContextMenuSurfaceManager::layerConfigurationRequested() const
{
    if (m_bundles.isEmpty())
        return false;
    for (const ContextMenuSurfaceBundle *bundle : m_bundles) {
        if (!bundle || !bundle->layerConfigurationRequested())
            return false;
    }
    return true;
}

QVariantMap ContextMenuSurfaceManager::overlayResidency() const
{
    bool created = false;
    bool visible = false;
    bool destroyPending = false;
    int width = 0;
    int height = 0;
    int liveInstances = 0;
    int visibleInstances = 0;
    quint64 createCount = 0;
    quint64 destroyCount = 0;
    for (const ContextMenuSurfaceBundle *bundle : m_bundles) {
        if (!bundle)
            continue;
        const QVariantMap snapshot = bundle->overlayResidency();
        const bool live = snapshot.value(QStringLiteral("created")).toBool();
        created = created || live;
        visible = visible || snapshot.value(QStringLiteral("visible")).toBool();
        destroyPending = destroyPending
            || snapshot.value(QStringLiteral("destroyPending")).toBool();
        visibleInstances += snapshot.value(QStringLiteral("visibleInstances")).toInt();
        if (live) {
            ++liveInstances;
            width = qMax(width, snapshot.value(QStringLiteral("width")).toInt());
            height = qMax(height, snapshot.value(QStringLiteral("height")).toInt());
        }
        createCount += snapshot.value(QStringLiteral("createCount")).toULongLong();
        destroyCount += snapshot.value(QStringLiteral("destroyCount")).toULongLong();
    }
    return {
        {QStringLiteral("class"), QStringLiteral("transient")},
        {QStringLiteral("created"), created},
        {QStringLiteral("visible"), visible},
        {QStringLiteral("destroyPending"), destroyPending},
        {QStringLiteral("width"), width},
        {QStringLiteral("height"), height},
        {QStringLiteral("liveInstances"), liveInstances},
        {QStringLiteral("visibleInstances"), visibleInstances},
        {QStringLiteral("createCount"), QVariant::fromValue<qulonglong>(createCount)},
        {QStringLiteral("destroyCount"), QVariant::fromValue<qulonglong>(destroyCount)},
    };
}

bool ContextMenuSurfaceManager::addScreen(QScreen *screen, QString *errorOut)
{
    if (m_stopped || !m_initialized || !screen || m_bundles.contains(screen))
        return true;
    auto *bundle = m_bundleFactory(screen, this);
    if (!bundle) {
        if (errorOut)
            *errorOut = QStringLiteral("Context menu surface bundle factory returned null");
        return false;
    }
    if (!bundle->initialize(errorOut)) {
        delete bundle;
        return false;
    }
    connect(bundle, &ContextMenuSurfaceBundle::mappingChanged,
            this, &ContextMenuSurfaceManager::syncMappingNotification);
    connect(bundle, &ContextMenuSurfaceBundle::surfaceError, this, [this](const QString &error) {
        qCritical("Astrea Context Menu surface creation failed: %s", qPrintable(error));
        m_application.exit(1);
    });
    const QPointer<QScreen> trackedScreen(screen);
    connect(screen, &QScreen::geometryChanged, this, [this, trackedScreen](const QRect &) {
        handleScreenGeometryChanged(trackedScreen.data());
    });
    m_bundles.insert(screen, bundle);
    bundle->map();
    syncMappingNotification();
    emit bundleCountChanged();
    emit layerStateChanged();
    return true;
}

void ContextMenuSurfaceManager::removeScreen(QScreen *screen)
{
    if (m_stopped || !m_initialized)
        return;
    auto it = m_bundles.find(screen);
    if (it == m_bundles.end())
        return;
    const QString key = it.value()->outputKey();
    if (m_controller)
        m_controller->invalidateOutput(key);
    QObject::disconnect(screen, &QScreen::geometryChanged, this, nullptr);
    auto *bundle = it.value();
    m_bundles.erase(it);
    delete bundle;
    syncMappingNotification();
    emit bundleCountChanged();
    emit layerStateChanged();
}

void ContextMenuSurfaceManager::handleScreenGeometryChanged(QScreen *screen)
{
    if (m_stopped || !m_initialized)
        return;
    if (auto it = m_bundles.constFind(screen); it != m_bundles.constEnd() && it.value())
        it.value()->updateForScreen();
}

void ContextMenuSurfaceManager::syncMappingNotification()
{
    const bool mapped = overlayMapped();
    if (mapped == m_lastOverlayMapped)
        return;
    m_lastOverlayMapped = mapped;
    emit mappingChanged();
}
