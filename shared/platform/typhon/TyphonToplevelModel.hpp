#pragma once

#include "platform/typhon/TyphonProtocolTypes.hpp"

#include <QObject>
#include <QHash>
#include <optional>

class TyphonToplevelModel final : public QObject {
    Q_OBJECT

public:
    enum class EventResult {
        Accepted,
        IgnoredStaleGeneration,
        Rejected
    };
    Q_ENUM(EventResult)

    explicit TyphonToplevelModel(QObject *parent = nullptr);

    EventResult startGeneration(quint64 generation);

    EventResult handleCreated(quint64 generation, quint64 token);
    EventResult identifierChanged(quint64 generation, quint64 token, const QString &id);
    EventResult appIdChanged(quint64 generation, quint64 token, const QString &appId);
    EventResult titleChanged(quint64 generation, quint64 token, const QString &title);
    EventResult pidChanged(quint64 generation, quint64 token, quint32 pid);
    EventResult kindChanged(quint64 generation, quint64 token, Astrea::Typhon::ToplevelKind kind);
    EventResult stateChanged(quint64 generation, quint64 token,
                             Astrea::Typhon::ToplevelStates states,
                             quint32 rawStateBits = 0);
    EventResult focusSerialChanged(quint64 generation, quint64 token,
                                   Astrea::Typhon::FocusSerial serial);
    EventResult handleDone(quint64 generation, quint64 token,
                           Astrea::Typhon::Revision revision);
    EventResult handleClosed(quint64 generation, quint64 token);
    EventResult managerDone(quint64 generation, Astrea::Typhon::Revision revision,
                            quint32 total, bool truncated);
    EventResult managerFailed(quint64 generation);

    void clearSnapshot(quint64 generation);

    const Astrea::Typhon::Snapshot &snapshot() const { return m_snapshot; }
    bool hasCommittedSnapshot() const { return m_hasSnapshot; }
    bool hasPendingTransaction() const { return !m_pending.isEmpty(); }
    bool isDegraded() const { return m_degraded; }
    quint64 currentGeneration() const { return m_generation; }
    Astrea::Typhon::Revision lastCommittedRevision() const { return m_lastRevision; }
    QString lastError() const { return m_lastError; }

signals:
    void snapshotCommitted(Astrea::Typhon::Snapshot snapshot);
    void degraded(QString diagnostic);

private:
    struct PendingHandleState {
        std::optional<QString> id;
        std::optional<QString> appId;
        std::optional<QString> title;
        std::optional<quint32> pid;
        std::optional<Astrea::Typhon::ToplevelKind> kind;
        std::optional<Astrea::Typhon::ToplevelStates> states;
        std::optional<quint32> rawStateBits;
        std::optional<Astrea::Typhon::FocusSerial> focusSerial;
        bool closed = false;
        bool complete = false;
        std::optional<Astrea::Typhon::Revision> doneRevision;
    };

    EventResult checkGeneration(quint64 generation) const;
    EventResult reject(const QString &diagnostic);
    PendingHandleState *pendingForUpdate(quint64 token);
    const PendingHandleState *pendingFor(quint64 token) const;
    bool validateComplete(const PendingHandleState &pending) const;
    std::optional<Astrea::Typhon::Toplevel> toToplevel(const PendingHandleState &pending) const;
    void sortWindows(QVector<Astrea::Typhon::Toplevel> &windows) const;

    quint64 m_generation = 0;
    bool m_generationStarted = false;
    bool m_degraded = false;
    bool m_hasSnapshot = false;
    Astrea::Typhon::Revision m_lastRevision = 0;
    QString m_lastError;
    Astrea::Typhon::Snapshot m_snapshot;
    QHash<quint64, Astrea::Typhon::Toplevel> m_committed;
    QHash<quint64, PendingHandleState> m_pending;
};
