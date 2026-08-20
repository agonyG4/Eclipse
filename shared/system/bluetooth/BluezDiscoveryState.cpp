#include "system/bluetooth/BluezDiscoveryState.hpp"

namespace Astrea::System {

void BluezDiscoveryState::reset()
{
    m_owners.clear();
    m_lease = BluezDiscoveryLease::None;
    m_adapterReady = false;
    m_actualDiscovering = false;
}

void BluezDiscoveryState::request(const QString &owner)
{
    if (!owner.isEmpty())
        m_owners.insert(owner);
}

void BluezDiscoveryState::release(const QString &owner)
{
    m_owners.remove(owner);
}

void BluezDiscoveryState::setAdapterReady(bool ready)
{
    m_adapterReady = ready;
    if (!ready)
        m_lease = BluezDiscoveryLease::None;
}

void BluezDiscoveryState::setActualDiscovering(bool discovering)
{
    m_actualDiscovering = discovering;
}

void BluezDiscoveryState::startRequested()
{
    if (wantsStart())
        m_lease = BluezDiscoveryLease::StartPending;
}

void BluezDiscoveryState::stopRequested()
{
    if (wantsStop())
        m_lease = BluezDiscoveryLease::StopPending;
}

void BluezDiscoveryState::operationFinished(bool start, bool success)
{
    if (start && m_lease == BluezDiscoveryLease::StartPending) {
        m_lease = success ? BluezDiscoveryLease::Held : BluezDiscoveryLease::None;
    } else if (!start && m_lease == BluezDiscoveryLease::StopPending) {
        m_lease = BluezDiscoveryLease::None;
    }
}

bool BluezDiscoveryState::wantsStart() const
{
    return m_adapterReady && hasDemand() && m_lease == BluezDiscoveryLease::None;
}

bool BluezDiscoveryState::wantsStop() const
{
    return !hasDemand() && m_lease == BluezDiscoveryLease::Held;
}

} // namespace Astrea::System
