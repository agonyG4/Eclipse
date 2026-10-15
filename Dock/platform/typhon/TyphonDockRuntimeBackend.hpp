#pragma once

#include "core/runtime/DockRuntimeBackend.hpp"

class TyphonToplevelConnection;

namespace Astrea::Dock {

class TyphonDockRuntimeBackend final : public DockRuntimeBackend {
    Q_OBJECT

public:
    explicit TyphonDockRuntimeBackend(TyphonToplevelConnection *connection,
                                      QObject *parent = nullptr);

    bool runtimeReady() const override { return m_runtimeReady; }
    bool actionReady() const override { return m_actionReady; }
    DockRuntimeSnapshot snapshot() const override { return m_snapshot; }
    quint64 generation() const override;
    std::optional<quint64> handleTokenForWindowId(const QString &windowId) const override;

    std::optional<DockRuntimeActionError> requestAction(
        const QString &windowId, DockRuntimeAction action, quint64 consumerToken) override;
    std::optional<DockRuntimeActionError> setMinimizeAnchor(
        const QString &windowId, const QRect &rect) override;
    std::optional<DockRuntimeActionError> clearMinimizeAnchor(
        const QString &windowId) override;

    QString diagnosticContext() const override;

private:
    void refreshRuntimeState(bool publishSnapshot);
    void refreshActionReadiness();

    TyphonToplevelConnection *m_connection = nullptr;
    DockRuntimeSnapshot m_snapshot;
    bool m_runtimeReady = false;
    bool m_actionReady = false;
};

} // namespace Astrea::Dock
