#pragma once

#include "core/runtime/DockRuntimeTypes.hpp"

#include <QObject>
#include <QRect>

#include <optional>

namespace Astrea::Dock {

class DockRuntimeBackend : public QObject {
    Q_OBJECT

public:
    explicit DockRuntimeBackend(QObject *parent = nullptr);
    ~DockRuntimeBackend() override = default;

    virtual bool runtimeReady() const = 0;
    virtual bool actionReady() const = 0;
    virtual DockRuntimeSnapshot snapshot() const = 0;
    virtual quint64 generation() const = 0;
    virtual std::optional<quint64> handleTokenForWindowId(const QString &windowId) const = 0;

    virtual std::optional<DockRuntimeActionError> requestAction(
        const QString &windowId, DockRuntimeAction action, quint64 consumerToken) = 0;
    virtual std::optional<DockRuntimeActionError> setMinimizeAnchor(
        const QString &windowId, const QRect &rect) = 0;
    virtual std::optional<DockRuntimeActionError> clearMinimizeAnchor(
        const QString &windowId) = 0;

    // The adapter formats this opaque suffix to preserve the legacy diagnostic
    // tokens without exposing transport state enums to Dock core.
    virtual QString diagnosticContext() const = 0;

signals:
    // Delivers runtime content changes, including authority loss via a non-authoritative snapshot.
    void snapshotChanged(Astrea::Dock::DockRuntimeSnapshot snapshot);
    // Reports readiness transitions only; becoming ready does not replay snapshot().
    void runtimeReadyChanged(bool ready);
    void actionReadyChanged(bool ready);
    void actionFinished(quint64 consumerToken, Astrea::Dock::DockRuntimeAction action,
                        Astrea::Dock::DockRuntimeActionResult result);
    void actionFailed(quint64 consumerToken, Astrea::Dock::DockRuntimeAction action,
                      Astrea::Dock::DockRuntimeActionError error);
};

} // namespace Astrea::Dock
