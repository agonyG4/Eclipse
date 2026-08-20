#pragma once

#include <QSet>

namespace Astrea::System {

enum class BluezDiscoveryLease {
    None,
    StartPending,
    Held,
    StopPending,
};

class BluezDiscoveryState final {
public:
    void reset();
    void request(const QString &owner);
    void release(const QString &owner);
    void setAdapterReady(bool ready);
    void setActualDiscovering(bool discovering);
    void startRequested();
    void stopRequested();
    void operationFinished(bool start, bool success);

    bool hasDemand() const { return !m_owners.isEmpty(); }
    bool actualDiscovering() const { return m_actualDiscovering; }
    BluezDiscoveryLease lease() const { return m_lease; }
    bool wantsStart() const;
    bool wantsStop() const;
    const QSet<QString> &owners() const { return m_owners; }

private:
    QSet<QString> m_owners;
    BluezDiscoveryLease m_lease = BluezDiscoveryLease::None;
    bool m_adapterReady = false;
    bool m_actualDiscovering = false;
};

} // namespace Astrea::System
