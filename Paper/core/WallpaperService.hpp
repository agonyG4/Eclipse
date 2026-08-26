#pragma once

#include "WallpaperPersistence.hpp"
#include "WallpaperCatalog.hpp"
#include "WallpaperResolver.hpp"
#include "WallpaperSourceWatcher.hpp"
#include "WallpaperValidationWorker.hpp"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>

#include <memory>
#include <optional>

class QThread;

namespace Paper {

enum class WallpaperState {
    Ready,
    Loading,
    Fallback,
    Error,
};

enum class WallpaperFallback {
    None,
    SourceMissing,
    SourceUnreadable,
    UnsupportedKind,
    UnsupportedScope,
    InvalidDescriptor,
    FactoryDefaultUnavailable,
    EmergencyFallback,
};

using WallpaperOperationId = quint64;

enum class WallpaperOperationStatus {
    Succeeded,
    Rejected,
    Superseded,
    PersistenceFailed,
    CancelledByReset,
    TimedOut,
    Shutdown,
};

struct WallpaperSnapshot final
{
    std::optional<WallpaperDescriptor> configured;
    WallpaperDescriptor factoryDefault;
    WallpaperDescriptor effective;
    WallpaperState state = WallpaperState::Error;
    WallpaperFallback fallback = WallpaperFallback::FactoryDefaultUnavailable;
    quint64 generation = 0;
    QString errorCode;
    QString lastError;

    QJsonObject toJson() const;
};

struct WallpaperOperationResult final
{
    WallpaperOperationId id = 0;
    WallpaperOperationStatus status = WallpaperOperationStatus::Rejected;
    QString errorCode;
    QString message;
    WallpaperSnapshot snapshot;

    bool succeeded() const { return status == WallpaperOperationStatus::Succeeded; }
    QJsonObject toJson() const;
};

class WallpaperService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString configuredSource READ configuredSource NOTIFY configuredWallpaperChanged)
    Q_PROPERTY(QString effectiveSource READ effectiveSource NOTIFY effectiveWallpaperChanged)
    Q_PROPERTY(QString stateName READ stateName NOTIFY stateChanged)
    Q_PROPERTY(qulonglong generation READ generation NOTIFY generationChanged)

public:
    explicit WallpaperService(WallpaperResolver resolver,
                              std::unique_ptr<WallpaperPersistence> persistence,
                              QObject *parent = nullptr);
    WallpaperService(WallpaperResolver resolver,
                     std::unique_ptr<WallpaperPersistence> persistence,
                     std::shared_ptr<WallpaperCatalog> catalog,
                     QObject *parent = nullptr);
    ~WallpaperService() override;

    const WallpaperSnapshot &snapshot() const;
    QString configuredSource() const;
    QString effectiveSource() const;
    QString stateName() const;
    quint64 generation() const;
    QVariantMap snapshotMap() const;
    QVector<WallpaperDescriptor> listWallpapers();
    WallpaperDescriptor defaultWallpaper() const { return m_snapshot.factoryDefault; }
    int validationWorkCountForTests() const;
    int sourceWatchPathCountForTests() const
    {
        return m_sourceWatcher ? m_sourceWatcher->watchedPathCountForTests() : 0;
    }

    void initialize();
    WallpaperOperationId setWallpaper(const WallpaperDescriptor &descriptor);
    WallpaperOperationId selectWallpaper(const QString &logicalId,
                                         WallpaperFit fit = WallpaperFit::Cover);
    WallpaperOperationId importWallpaper(const QString &source,
                                         WallpaperFit fit = WallpaperFit::Cover,
                                         const QString &displayName = {});
    WallpaperOperationId addWallpaper(const QString &source,
                                      const QString &displayName = {});
    WallpaperOperationId resetWallpaper();
    void reload();

    Q_INVOKABLE qulonglong setWallpaperSource(const QString &source,
                                              const QString &fit = QStringLiteral("cover"));
    Q_INVOKABLE qulonglong reset();
    Q_INVOKABLE QVariantMap snapshotVariant() const;

signals:
    void configuredWallpaperChanged();
    void effectiveWallpaperChanged();
    void stateChanged();
    void fallbackChanged();
    void errorChanged();
    void generationChanged();
    void snapshotChanged();
    void wallpaperOperationFinished(quint64 id, Paper::WallpaperOperationResult result);
    void wallpaperChanged(Paper::WallpaperDescriptor previous,
                          Paper::WallpaperDescriptor current,
                          qulonglong generation,
                          QString reason);

private:
    enum class ValidationReason {
        Initial,
        Mutation,
        Reconcile,
    };

    struct PendingValidation final
    {
        quint64 token = 0;
        std::optional<WallpaperOperationId> operationId;
        ValidationReason reason = ValidationReason::Mutation;
        WallpaperDescriptor descriptor;
    };

private slots:
    void handleValidationResult(quint64 token, WallpaperResolution result);
    void reconcileConfiguredSource();

private:
    static WallpaperFallback fallbackFor(WallpaperResolutionError error);
    static QString errorCodeFor(WallpaperResolutionError error);
    void publishEffective(const WallpaperDescriptor &effective,
                          WallpaperState state,
                          WallpaperFallback fallback,
                          const QString &errorCode,
                          const QString &errorMessage);
    void emitSnapshotChanges(const WallpaperSnapshot &before,
                             bool forceEffectiveChange = false,
                             const QString &reason = QStringLiteral("runtime"));
    void setFailure(WallpaperResolutionError error, const QString &message);
    void dispatchNextValidation();
    void queueValidation(const WallpaperDescriptor &descriptor,
                         ValidationReason reason,
                         std::optional<WallpaperOperationId> operationId = std::nullopt);
    void supersedeOutstandingOperations(WallpaperOperationStatus status,
                                        const QString &errorCode,
                                        const QString &message);
    void dispatchQueuedReconcile();
    void startOperationDeadline(WallpaperOperationId id);
    void handleOperationTimeout(WallpaperOperationId id);
    void finishOperation(WallpaperOperationId id,
                         WallpaperOperationStatus status,
                         const WallpaperSnapshot &snapshot,
                         const QString &errorCode = {},
                         const QString &message = {});

    WallpaperResolver m_resolver;
    std::unique_ptr<WallpaperPersistence> m_persistence;
    std::shared_ptr<WallpaperCatalog> m_catalog;
    WallpaperSourceWatcher *m_sourceWatcher = nullptr;
    WallpaperSnapshot m_snapshot;
    bool m_initialized = false;
    QThread *m_validationThread = nullptr;
    WallpaperValidationWorker *m_validationWorker = nullptr;
    bool m_validationActive = false;
    quint64 m_activeValidationToken = 0;
    std::optional<WallpaperOperationId> m_activeOperationId;
    ValidationReason m_activeValidationReason = ValidationReason::Initial;
    quint64 m_latestRequestToken = 0;
    WallpaperOperationId m_nextOperationId = 0;
    std::optional<PendingValidation> m_pendingValidation;
    std::optional<WallpaperSnapshot> m_validationBase;
    bool m_reconcileQueued = false;
    QHash<WallpaperOperationId, QTimer *> m_operationTimers;
};

} // namespace Paper

Q_DECLARE_METATYPE(Paper::WallpaperSnapshot)
Q_DECLARE_METATYPE(Paper::WallpaperOperationResult)
