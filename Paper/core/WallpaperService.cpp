#include "WallpaperService.hpp"

#include "shared/platform/paper/PaperProtocol.hpp"

#include <QJsonArray>
#include <QMetaObject>
#include <QThread>

namespace Paper {
namespace {

QString stateToString(const WallpaperState state)
{
    switch (state) {
    case WallpaperState::Ready:
        return QStringLiteral("ready");
    case WallpaperState::Loading:
        return QStringLiteral("loading");
    case WallpaperState::Fallback:
        return QStringLiteral("fallback");
    case WallpaperState::Error:
        return QStringLiteral("error");
    }
    return QStringLiteral("error");
}

QString fallbackToString(const WallpaperFallback fallback)
{
    switch (fallback) {
    case WallpaperFallback::None:
        return QStringLiteral("none");
    case WallpaperFallback::SourceMissing:
        return QStringLiteral("source-missing");
    case WallpaperFallback::SourceUnreadable:
        return QStringLiteral("source-unreadable");
    case WallpaperFallback::UnsupportedKind:
        return QStringLiteral("unsupported-kind");
    case WallpaperFallback::UnsupportedScope:
        return QStringLiteral("unsupported-scope");
    case WallpaperFallback::InvalidDescriptor:
        return QStringLiteral("invalid-descriptor");
    case WallpaperFallback::FactoryDefaultUnavailable:
        return QStringLiteral("factory-default-unavailable");
    case WallpaperFallback::EmergencyFallback:
        return QStringLiteral("emergency-fallback");
    }
    return QStringLiteral("factory-default-unavailable");
}

QString operationStatusToString(const WallpaperOperationStatus status)
{
    switch (status) {
    case WallpaperOperationStatus::Succeeded:
        return QStringLiteral("succeeded");
    case WallpaperOperationStatus::Rejected:
        return QStringLiteral("rejected");
    case WallpaperOperationStatus::Superseded:
        return QStringLiteral("superseded");
    case WallpaperOperationStatus::PersistenceFailed:
        return QStringLiteral("persistence-failed");
    case WallpaperOperationStatus::CancelledByReset:
        return QStringLiteral("cancelled-by-reset");
    case WallpaperOperationStatus::TimedOut:
        return QStringLiteral("timed-out");
    case WallpaperOperationStatus::Shutdown:
        return QStringLiteral("shutdown");
    }
    return QStringLiteral("rejected");
}

} // namespace

QJsonObject WallpaperSnapshot::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("configured"), configured.has_value()
                                                ? QJsonValue(configured->toJson())
                                                : QJsonValue(QJsonValue::Null));
    json.insert(QStringLiteral("factoryDefault"), factoryDefault.toJson());
    json.insert(QStringLiteral("effective"), effective.toJson());
    json.insert(QStringLiteral("state"), stateToString(state));
    json.insert(QStringLiteral("fallback"), fallbackToString(fallback));
    json.insert(QStringLiteral("generation"), static_cast<qint64>(generation));
    json.insert(QStringLiteral("errorCode"), errorCode);
    json.insert(QStringLiteral("lastError"), lastError);
    return json;
}

QJsonObject WallpaperOperationResult::toJson() const
{
    QJsonObject json{
        {QStringLiteral("requestId"), static_cast<qint64>(id)},
        {QStringLiteral("status"), operationStatusToString(status)},
        {QStringLiteral("snapshot"), snapshot.toJson()},
    };
    if (!errorCode.isEmpty()) {
        json.insert(QStringLiteral("errorCode"), errorCode);
    }
    if (!message.isEmpty()) {
        json.insert(QStringLiteral("message"), message);
    }
    return json;
}

WallpaperService::WallpaperService(WallpaperResolver resolver,
                                   std::unique_ptr<WallpaperPersistence> persistence,
                                   QObject *parent)
    : WallpaperService(std::move(resolver), std::move(persistence), nullptr, parent)
{
}

WallpaperService::WallpaperService(WallpaperResolver resolver,
                                   std::unique_ptr<WallpaperPersistence> persistence,
                                   std::shared_ptr<WallpaperCatalog> catalog,
                                   QObject *parent)
    : QObject(parent)
    , m_resolver(std::move(resolver))
    , m_persistence(std::move(persistence))
    , m_catalog(std::move(catalog))
{
    qRegisterMetaType<WallpaperDescriptor>("Paper::WallpaperDescriptor");
    qRegisterMetaType<WallpaperResolution>("Paper::WallpaperResolution");
    qRegisterMetaType<WallpaperOperationResult>("Paper::WallpaperOperationResult");
    m_sourceWatcher = new WallpaperSourceWatcher(this);
    connect(m_sourceWatcher,
            &WallpaperSourceWatcher::sourceChanged,
            this,
            &WallpaperService::reconcileConfiguredSource);
    m_validationThread = new QThread(this);
    m_validationWorker = new WallpaperValidationWorker(m_resolver);
    m_validationWorker->moveToThread(m_validationThread);
    connect(m_validationWorker,
            &WallpaperValidationWorker::validated,
            this,
            &WallpaperService::handleValidationResult,
            Qt::QueuedConnection);
    m_validationThread->start();
}

WallpaperService::~WallpaperService()
{
    supersedeOutstandingOperations(WallpaperOperationStatus::Shutdown,
                                   QStringLiteral("shutdown"),
                                   QStringLiteral("Paper wallpaper service is shutting down"));
    if (!m_validationThread) {
        return;
    }
    m_validationThread->quit();
    m_validationThread->wait();
    delete m_validationWorker;
    m_validationWorker = nullptr;
}

const WallpaperSnapshot &WallpaperService::snapshot() const
{
    return m_snapshot;
}

QString WallpaperService::configuredSource() const
{
    return m_snapshot.configured ? m_snapshot.configured->source() : QString();
}

QString WallpaperService::effectiveSource() const
{
    return m_snapshot.effective.resolvedSource().isEmpty() ? m_snapshot.effective.source()
                                                             : m_snapshot.effective.resolvedSource();
}

QString WallpaperService::stateName() const
{
    return stateToString(m_snapshot.state);
}

quint64 WallpaperService::generation() const
{
    return m_snapshot.generation;
}

int WallpaperService::validationWorkCountForTests() const
{
    return (m_validationActive ? 1 : 0) + (m_pendingValidation.has_value() ? 1 : 0);
}

QVariantMap WallpaperService::snapshotMap() const
{
    const auto json = m_snapshot.toJson();
    QVariantMap result;
    for (auto it = json.constBegin(); it != json.constEnd(); ++it) {
        result.insert(it.key(), it.value().toVariant());
    }
    return result;
}

QVariantMap WallpaperService::snapshotVariant() const
{
    return snapshotMap();
}

QVector<WallpaperDescriptor> WallpaperService::listWallpapers()
{
    if (!m_catalog) {
        return {};
    }
    m_catalog->refresh();
    return m_catalog->list();
}

void WallpaperService::initialize()
{
    if (m_initialized) {
        return;
    }
    m_initialized = true;
    const auto before = m_snapshot;

    QString persistenceError;
    if (m_persistence) {
        if (m_catalog) {
            if (const auto selection = m_persistence->loadSelection(&persistenceError)) {
                m_snapshot.configured = m_catalog->resolve(selection->wallpaperId);
                if (m_snapshot.configured) {
                    m_snapshot.configured->setFit(selection->fit);
                } else if (const auto descriptor = m_persistence->load(&persistenceError)) {
                    auto missing = *descriptor;
                    missing.setLogicalId(selection->wallpaperId);
                    missing.setFit(selection->fit);
                    m_snapshot.configured = missing;
                } else {
                    auto missing = WallpaperDescriptor::externalFile(selection->wallpaperId,
                                                                      selection->fit);
                    missing.setLogicalId(selection->wallpaperId);
                    m_snapshot.configured = missing;
                }
            }
        } else {
            m_snapshot.configured = m_persistence->load(&persistenceError);
        }
        if (!m_snapshot.configured) {
            if (auto *xdgPersistence = dynamic_cast<XdgWallpaperPersistence *>(m_persistence.get())) {
                if (const auto legacy = xdgPersistence->migrateLegacy()) {
                    if (m_catalog) {
                        QString importError;
                        if (auto imported = m_catalog->importWallpaper(legacy->source(),
                                                                        &importError)) {
                            imported->setFit(legacy->fit());
                            m_snapshot.configured = imported;
                        } else {
                            m_snapshot.configured = legacy;
                            if (persistenceError.isEmpty()) {
                                persistenceError = importError;
                            }
                        }
                    } else {
                        m_snapshot.configured = legacy;
                    }
                }
            }
        }
    }

    const auto factory = m_resolver.factoryDefault();
    m_snapshot.factoryDefault = factory.descriptor;
    m_snapshot.effective = factory.descriptor;
    m_snapshot.state = factory.ok() ? (factory.error == WallpaperResolutionError::EmergencyFallback
                                           ? WallpaperState::Fallback
                                           : WallpaperState::Ready)
                                    : WallpaperState::Error;
    m_snapshot.fallback = factory.error == WallpaperResolutionError::EmergencyFallback
        ? WallpaperFallback::EmergencyFallback
        : factory.ok() ? WallpaperFallback::None
                       : WallpaperFallback::FactoryDefaultUnavailable;
    m_snapshot.errorCode = factory.ok() ? QString() : errorCodeFor(factory.error);
    m_snapshot.lastError = persistenceError;

    if (!m_snapshot.effective.source().isEmpty()) {
        m_snapshot.generation = 1;
    }
    emitSnapshotChanges(before);
    if (m_snapshot.configured) {
        m_sourceWatcher->setSource(*m_snapshot.configured);
        queueValidation(*m_snapshot.configured, ValidationReason::Initial);
    }
}

WallpaperOperationId WallpaperService::setWallpaper(const WallpaperDescriptor &descriptor)
{
    if (!m_initialized) {
        initialize();
    }

    const auto operationId = ++m_nextOperationId;
    supersedeOutstandingOperations(WallpaperOperationStatus::Superseded,
                                   QStringLiteral("superseded"),
                                   QStringLiteral("A newer wallpaper request superseded this request"));
    queueValidation(descriptor, ValidationReason::Mutation, operationId);
    startOperationDeadline(operationId);
    return operationId;
}

WallpaperOperationId WallpaperService::selectWallpaper(const QString &logicalId,
                                                        const WallpaperFit fit)
{
    if (!m_catalog) {
        setFailure(WallpaperResolutionError::InvalidDescriptor,
                   QStringLiteral("Wallpaper catalog is unavailable"));
        return 0;
    }
    const auto descriptor = m_catalog->resolve(logicalId);
    if (!descriptor) {
        setFailure(WallpaperResolutionError::SourceMissing,
                   QStringLiteral("Unknown wallpaper ID: %1").arg(logicalId));
        return 0;
    }
    auto selected = *descriptor;
    selected.setFit(fit);
    return setWallpaper(selected);
}

WallpaperOperationId WallpaperService::importWallpaper(const QString &source,
                                                        const WallpaperFit fit)
{
    if (!m_catalog) {
        setFailure(WallpaperResolutionError::InvalidDescriptor,
                   QStringLiteral("Wallpaper catalog is unavailable"));
        return 0;
    }
    QString error;
    const auto imported = m_catalog->importWallpaper(source, &error);
    if (!imported) {
        setFailure(WallpaperResolutionError::UnsupportedImage,
                   error.isEmpty() ? QStringLiteral("Wallpaper import failed") : error);
        return 0;
    }
    auto selected = *imported;
    selected.setFit(fit);
    return setWallpaper(selected);
}

WallpaperOperationId WallpaperService::resetWallpaper()
{
    if (!m_initialized) {
        initialize();
    }
    const auto operationId = ++m_nextOperationId;
    supersedeOutstandingOperations(WallpaperOperationStatus::CancelledByReset,
                                   QStringLiteral("cancelled-by-reset"),
                                   QStringLiteral("Wallpaper request was cancelled by reset"));
    ++m_latestRequestToken;
    m_pendingValidation.reset();
    m_validationBase.reset();

    QString persistenceError;
    if (m_persistence && !m_persistence->clear(&persistenceError)) {
        setFailure(WallpaperResolutionError::FactoryDefaultUnavailable, persistenceError);
        finishOperation(operationId,
                        WallpaperOperationStatus::PersistenceFailed,
                        m_snapshot,
                        QStringLiteral("persistence-write-failed"),
                        persistenceError);
        return operationId;
    }

    const auto before = m_snapshot;
    m_snapshot.configured.reset();
    m_snapshot.effective = m_snapshot.factoryDefault;
    const bool emergency = m_snapshot.factoryDefault.logicalId()
        == QStringLiteral("astrea://wallpaper/emergency");
    m_snapshot.state = m_snapshot.factoryDefault.source().isEmpty()
        ? WallpaperState::Error
        : emergency ? WallpaperState::Fallback : WallpaperState::Ready;
    m_snapshot.fallback = m_snapshot.state == WallpaperState::Error
        ? WallpaperFallback::FactoryDefaultUnavailable
        : emergency ? WallpaperFallback::EmergencyFallback : WallpaperFallback::None;
    m_snapshot.errorCode = m_snapshot.state == WallpaperState::Error
        ? QStringLiteral("factory-default-unavailable")
        : emergency ? QStringLiteral("emergency-fallback") : QString();
    m_snapshot.lastError.clear();
    m_sourceWatcher->clear();
    m_reconcileQueued = false;
    if (before.effective != m_snapshot.effective) {
        ++m_snapshot.generation;
    }
    emitSnapshotChanges(before);
    finishOperation(operationId, WallpaperOperationStatus::Succeeded, m_snapshot);
    return operationId;
}

void WallpaperService::reload()
{
    if (!m_initialized) {
        initialize();
        return;
    }
    const auto before = m_snapshot;
    const auto factory = m_resolver.factoryDefault();
    m_snapshot.factoryDefault = factory.descriptor;
    if (m_snapshot.configured) {
        const auto result = m_resolver.resolve(*m_snapshot.configured);
        if (result.ok() && result.error != WallpaperResolutionError::EmergencyFallback) {
            m_snapshot.effective = result.descriptor;
            m_snapshot.state = WallpaperState::Ready;
            m_snapshot.fallback = WallpaperFallback::None;
            m_snapshot.lastError.clear();
            m_snapshot.errorCode.clear();
        } else {
            m_snapshot.effective = factory.descriptor;
            m_snapshot.state = WallpaperState::Fallback;
            m_snapshot.fallback = fallbackFor(result.error);
            m_snapshot.errorCode = errorCodeFor(result.error);
            m_snapshot.lastError = result.message;
        }
    } else {
        m_snapshot.effective = factory.descriptor;
        m_snapshot.state = !factory.ok() ? WallpaperState::Error
                                         : factory.error == WallpaperResolutionError::EmergencyFallback
            ? WallpaperState::Fallback
            : WallpaperState::Ready;
        m_snapshot.fallback = !factory.ok()
            ? WallpaperFallback::FactoryDefaultUnavailable
            : factory.error == WallpaperResolutionError::EmergencyFallback
            ? WallpaperFallback::EmergencyFallback
            : WallpaperFallback::None;
    }
    if (before.effective != m_snapshot.effective) {
        ++m_snapshot.generation;
    }
    emitSnapshotChanges(before);
}

qulonglong WallpaperService::setWallpaperSource(const QString &source, const QString &fit)
{
    const auto parsedFit = wallpaperFitFromStringStrict(fit);
    if (!parsedFit) {
        setFailure(WallpaperResolutionError::InvalidDescriptor,
                   QStringLiteral("Wallpaper fit is not supported"));
        return 0;
    }
    return importWallpaper(source, *parsedFit);
}

qulonglong WallpaperService::reset()
{
    return resetWallpaper();
}

WallpaperFallback WallpaperService::fallbackFor(const WallpaperResolutionError error)
{
    switch (error) {
    case WallpaperResolutionError::SourceMissing:
        return WallpaperFallback::SourceMissing;
    case WallpaperResolutionError::SourceNotRegularFile:
    case WallpaperResolutionError::SourceUnreadable:
    case WallpaperResolutionError::UnsupportedImage:
        return WallpaperFallback::SourceUnreadable;
    case WallpaperResolutionError::UnsupportedKind:
        return WallpaperFallback::UnsupportedKind;
    case WallpaperResolutionError::UnsupportedScope:
        return WallpaperFallback::UnsupportedScope;
    case WallpaperResolutionError::InvalidDescriptor:
    case WallpaperResolutionError::InvalidUri:
        return WallpaperFallback::InvalidDescriptor;
    case WallpaperResolutionError::EmergencyFallback:
        return WallpaperFallback::EmergencyFallback;
    case WallpaperResolutionError::FactoryDefaultUnavailable:
        return WallpaperFallback::FactoryDefaultUnavailable;
    case WallpaperResolutionError::None:
        return WallpaperFallback::None;
    }
    return WallpaperFallback::FactoryDefaultUnavailable;
}

QString WallpaperService::errorCodeFor(const WallpaperResolutionError error)
{
    switch (error) {
    case WallpaperResolutionError::None:
        return {};
    case WallpaperResolutionError::InvalidDescriptor:
        return QStringLiteral("invalid-descriptor");
    case WallpaperResolutionError::InvalidUri:
        return QStringLiteral("invalid-uri");
    case WallpaperResolutionError::UnsupportedKind:
        return QStringLiteral("unsupported-kind");
    case WallpaperResolutionError::UnsupportedScope:
        return QStringLiteral("unsupported-scope");
    case WallpaperResolutionError::SourceMissing:
        return QStringLiteral("source-missing");
    case WallpaperResolutionError::SourceNotRegularFile:
        return QStringLiteral("source-not-regular-file");
    case WallpaperResolutionError::SourceUnreadable:
        return QStringLiteral("source-unreadable");
    case WallpaperResolutionError::UnsupportedImage:
        return QStringLiteral("unsupported-image");
    case WallpaperResolutionError::FactoryDefaultUnavailable:
        return QStringLiteral("factory-default-unavailable");
    case WallpaperResolutionError::EmergencyFallback:
        return QStringLiteral("emergency-fallback");
    }
    return QStringLiteral("unknown-error");
}

void WallpaperService::publishEffective(const WallpaperDescriptor &effective,
                                        const WallpaperState state,
                                        const WallpaperFallback fallback,
                                        const QString &errorCode,
                                        const QString &errorMessage)
{
    const auto before = m_snapshot;
    m_snapshot.effective = effective;
    m_snapshot.state = state;
    m_snapshot.fallback = fallback;
    m_snapshot.errorCode = errorCode;
    m_snapshot.lastError = errorMessage;
    if (before.effective != m_snapshot.effective) {
        ++m_snapshot.generation;
    }
    emitSnapshotChanges(before);
}

void WallpaperService::emitSnapshotChanges(const WallpaperSnapshot &before,
                                            const bool forceEffectiveChange,
                                            const QString &reason)
{
    const bool configuredChanged = before.configured.has_value() != m_snapshot.configured.has_value()
        || (before.configured && m_snapshot.configured
            && *before.configured != *m_snapshot.configured);
    const bool effectiveChanged = forceEffectiveChange || before.effective != m_snapshot.effective;
    if (configuredChanged) {
        emit configuredWallpaperChanged();
    }
    if (effectiveChanged) {
        emit effectiveWallpaperChanged();
        emit wallpaperChanged(before.effective,
                              m_snapshot.effective,
                              m_snapshot.generation,
                              reason);
    }
    if (before.state != m_snapshot.state) {
        emit stateChanged();
    }
    if (before.fallback != m_snapshot.fallback) {
        emit fallbackChanged();
    }
    if (before.errorCode != m_snapshot.errorCode || before.lastError != m_snapshot.lastError) {
        emit errorChanged();
    }
    if (before.generation != m_snapshot.generation) {
        emit generationChanged();
    }
    if (configuredChanged || effectiveChanged
        || before.state != m_snapshot.state || before.fallback != m_snapshot.fallback
        || before.errorCode != m_snapshot.errorCode || before.lastError != m_snapshot.lastError) {
        emit snapshotChanged();
    }
}

void WallpaperService::setFailure(const WallpaperResolutionError error, const QString &message)
{
    const auto before = m_snapshot;
    m_snapshot.errorCode = errorCodeFor(error);
    m_snapshot.lastError = message;
    emitSnapshotChanges(before);
}

void WallpaperService::queueValidation(const WallpaperDescriptor &descriptor,
                                       const ValidationReason reason,
                                       const std::optional<WallpaperOperationId> operationId)
{
    if (!m_validationBase && !m_pendingValidation) {
        m_validationBase = m_snapshot;
    }
    const auto before = m_snapshot;
    m_pendingValidation = PendingValidation{++m_latestRequestToken, operationId, reason, descriptor};
    m_snapshot.state = WallpaperState::Loading;
    m_snapshot.fallback = WallpaperFallback::None;
    m_snapshot.errorCode.clear();
    m_snapshot.lastError.clear();
    emitSnapshotChanges(before);
    dispatchNextValidation();
}

void WallpaperService::dispatchNextValidation()
{
    if (m_validationActive || !m_pendingValidation || !m_validationWorker) {
        return;
    }

    const auto request = *m_pendingValidation;
    m_pendingValidation.reset();
    m_validationActive = true;
    m_activeValidationToken = request.token;
    m_activeOperationId = request.operationId;
    m_activeValidationReason = request.reason;
    QMetaObject::invokeMethod(m_validationWorker,
                              "validate",
                              Qt::QueuedConnection,
                              Q_ARG(quint64, request.token),
                              Q_ARG(Paper::WallpaperDescriptor, request.descriptor));
}

void WallpaperService::handleValidationResult(const quint64 token, WallpaperResolution result)
{
    const auto operationId = m_activeOperationId;
    const auto reason = m_activeValidationReason;
    m_validationActive = false;
    m_activeOperationId.reset();
    if (token != m_activeValidationToken || token != m_latestRequestToken
        || m_pendingValidation.has_value()) {
        dispatchNextValidation();
        dispatchQueuedReconcile();
        return;
    }

    const auto base = m_validationBase.value_or(m_snapshot);
    m_validationBase.reset();
    if (!result.ok() || result.error == WallpaperResolutionError::EmergencyFallback) {
        if (reason == ValidationReason::Reconcile) {
            const auto before = m_snapshot;
            m_snapshot = base;
            m_snapshot.effective = m_snapshot.factoryDefault;
            m_snapshot.state = m_snapshot.effective.source().isEmpty() ? WallpaperState::Error
                                                                        : WallpaperState::Fallback;
            m_snapshot.fallback = fallbackFor(result.error);
            m_snapshot.errorCode = errorCodeFor(result.error);
            m_snapshot.lastError = result.message;
            if (before.effective != m_snapshot.effective) {
                ++m_snapshot.generation;
            }
            emitSnapshotChanges(before);
            dispatchQueuedReconcile();
            return;
        }

        const auto before = m_snapshot;
        m_snapshot = base;
        if (reason == ValidationReason::Initial) {
            m_snapshot.state = WallpaperState::Fallback;
            m_snapshot.fallback = fallbackFor(result.error);
        }
        m_snapshot.errorCode = errorCodeFor(result.error);
        m_snapshot.lastError = result.message;
        emitSnapshotChanges(before);
        if (reason == ValidationReason::Mutation && operationId) {
            finishOperation(*operationId,
                            WallpaperOperationStatus::Rejected,
                            m_snapshot,
                            errorCodeFor(result.error),
                            result.message);
        }
        dispatchQueuedReconcile();
        return;
    }

    QString persistenceError;
    bool persistenceSucceeded = true;
    if (reason != ValidationReason::Reconcile && m_persistence) {
        const bool hasStableCatalogId = m_catalog && !result.descriptor.logicalId().isEmpty()
            && result.descriptor.logicalId().startsWith(QStringLiteral("astrea://wallpaper/"));
        persistenceSucceeded = hasStableCatalogId
            ? m_persistence->saveSelection(
                  WallpaperSelection{result.descriptor.logicalId(), result.descriptor.fit()},
                  &persistenceError)
            : m_persistence->save(result.descriptor, &persistenceError);
    }
    if (reason != ValidationReason::Reconcile && !persistenceSucceeded) {
        const auto before = m_snapshot;
        m_snapshot = base;
        m_snapshot.errorCode = QStringLiteral("persistence-write-failed");
        m_snapshot.lastError = persistenceError.isEmpty()
            ? QStringLiteral("Could not persist wallpaper")
            : persistenceError;
        emitSnapshotChanges(before);
        if (reason == ValidationReason::Mutation && operationId) {
            finishOperation(*operationId,
                            WallpaperOperationStatus::PersistenceFailed,
                            m_snapshot,
                            QStringLiteral("persistence-write-failed"),
                            m_snapshot.lastError);
        }
        dispatchQueuedReconcile();
        return;
    }

    const auto before = m_snapshot;
    m_snapshot.configured = result.descriptor;
    m_snapshot.effective = result.descriptor;
    m_snapshot.state = WallpaperState::Ready;
    m_snapshot.fallback = WallpaperFallback::None;
    m_snapshot.errorCode.clear();
    m_snapshot.lastError.clear();
    m_sourceWatcher->setSource(result.descriptor);
    m_snapshot.generation = base.generation;
    if (base.effective != m_snapshot.effective || reason == ValidationReason::Reconcile) {
        ++m_snapshot.generation;
    }
    emitSnapshotChanges(before, reason == ValidationReason::Reconcile);
    if (reason == ValidationReason::Mutation && operationId) {
        finishOperation(*operationId, WallpaperOperationStatus::Succeeded, m_snapshot);
    }
    dispatchQueuedReconcile();
}

void WallpaperService::reconcileConfiguredSource()
{
    if (!m_initialized || !m_snapshot.configured) {
        return;
    }
    if (m_validationActive || m_pendingValidation.has_value()) {
        m_reconcileQueued = true;
        return;
    }
    queueValidation(*m_snapshot.configured, ValidationReason::Reconcile);
}

void WallpaperService::dispatchQueuedReconcile()
{
    if (!m_reconcileQueued || !m_initialized || !m_snapshot.configured
        || m_validationActive || m_pendingValidation.has_value()) {
        return;
    }
    m_reconcileQueued = false;
    queueValidation(*m_snapshot.configured, ValidationReason::Reconcile);
}

void WallpaperService::startOperationDeadline(const WallpaperOperationId id)
{
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, id] { handleOperationTimeout(id); });
    m_operationTimers.insert(id, timer);
    timer->start(Astrea::PaperProtocol::kOperationDeadlineMs);
}

void WallpaperService::handleOperationTimeout(const WallpaperOperationId id)
{
    auto *timer = m_operationTimers.take(id);
    if (!timer) {
        return;
    }
    timer->deleteLater();

    const auto base = m_validationBase.value_or(m_snapshot);
    const auto pendingIsOperation = m_pendingValidation && m_pendingValidation->operationId
        && *m_pendingValidation->operationId == id;
    const auto activeIsOperation = m_activeOperationId && *m_activeOperationId == id;
    if (!pendingIsOperation && !activeIsOperation) {
        return;
    }

    if (pendingIsOperation) {
        m_pendingValidation.reset();
    }
    if (activeIsOperation) {
        m_activeOperationId.reset();
        ++m_latestRequestToken;
    }

    const auto before = m_snapshot;
    if (!m_pendingValidation && !m_activeOperationId) {
        m_snapshot = base;
        emitSnapshotChanges(before);
        m_validationBase.reset();
    }
    finishOperation(id,
                    WallpaperOperationStatus::TimedOut,
                    m_snapshot,
                    QStringLiteral("operation-timeout"),
                    QStringLiteral("Wallpaper operation exceeded the Paper deadline"));
}

void WallpaperService::supersedeOutstandingOperations(const WallpaperOperationStatus status,
                                                       const QString &errorCode,
                                                       const QString &message)
{
    const auto authoritative = m_validationBase.value_or(m_snapshot);
    if (m_pendingValidation && m_pendingValidation->operationId) {
        finishOperation(*m_pendingValidation->operationId, status, authoritative, errorCode, message);
    }
    m_pendingValidation.reset();
    if (m_validationActive && m_activeOperationId) {
        finishOperation(*m_activeOperationId, status, authoritative, errorCode, message);
        m_activeOperationId.reset();
    }
    m_reconcileQueued = false;
}

void WallpaperService::finishOperation(const WallpaperOperationId id,
                                       const WallpaperOperationStatus status,
                                       const WallpaperSnapshot &snapshot,
                                       const QString &errorCode,
                                       const QString &message)
{
    if (auto *timer = m_operationTimers.take(id)) {
        timer->stop();
        timer->deleteLater();
    }
    WallpaperOperationResult result;
    result.id = id;
    result.status = status;
    result.errorCode = errorCode;
    result.message = message;
    result.snapshot = snapshot;
    emit wallpaperOperationFinished(id, result);
}

} // namespace Paper
