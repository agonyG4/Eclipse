#include "system/network/WifiNetworkModel.hpp"

#include <QHash>

#include <algorithm>

namespace Astrea::System {

WifiNetworkModel::WifiNetworkModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int WifiNetworkModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_networks.size();
}

QVariant WifiNetworkModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_networks.size())
        return {};
    const WifiNetwork &network = m_networks.at(index.row());
    switch (role) {
    case SsidRole: return network.ssid;
    case StrengthRole: return network.strength;
    case ActiveRole: return network.active;
    case SecuredRole: return network.secured;
    case FrequencyRole: return network.frequencyMHz;
    case BssidRole: return network.bssid;
    case Qt::DisplayRole: return network.ssid;
    default: return {};
    }
}

QHash<int, QByteArray> WifiNetworkModel::roleNames() const
{
    return {
        {SsidRole, "ssid"},
        {StrengthRole, "strength"},
        {ActiveRole, "active"},
        {SecuredRole, "secured"},
        {FrequencyRole, "frequencyMHz"},
        {BssidRole, "bssid"},
    };
}

void WifiNetworkModel::replace(QVector<WifiNetwork> networks)
{
    QHash<QString, WifiNetwork> bySsid;
    for (const WifiNetwork &network : networks) {
        if (!bySsid.contains(network.ssid)) {
            bySsid.insert(network.ssid, network);
            continue;
        }
        WifiNetwork &current = bySsid[network.ssid];
        const auto score = [](const WifiNetwork &value) {
            return std::tuple{value.active, value.strength, value.bssid};
        };
        if (score(network) > score(current))
            current = network;
    }
    networks = bySsid.values().toVector();
    std::sort(networks.begin(), networks.end(), [](const WifiNetwork &left,
                                                   const WifiNetwork &right) {
        if (left.active != right.active)
            return left.active > right.active;
        if (left.strength != right.strength)
            return left.strength > right.strength;
        return QString::compare(left.ssid, right.ssid, Qt::CaseInsensitive) < 0;
    });
    if (networks == m_networks)
        return;
    beginResetModel();
    m_networks = std::move(networks);
    endResetModel();
}

} // namespace Astrea::System
