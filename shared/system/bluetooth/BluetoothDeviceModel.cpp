#include "system/bluetooth/BluetoothDeviceModel.hpp"

#include <algorithm>

namespace Astrea::System {

BluetoothDeviceModel::BluetoothDeviceModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int BluetoothDeviceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_devices.size();
}

QVariant BluetoothDeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_devices.size())
        return {};
    const BluetoothDevice &device = m_devices.at(index.row());
    switch (role) {
    case IdRole: return device.id;
    case ObjectPathRole: return device.objectPath;
    case AddressRole: return device.address;
    case NameRole: return device.name;
    case PairedRole: return device.paired;
    case TrustedRole: return device.trusted;
    case ConnectedRole: return device.connected;
    case DiscoveredRole: return device.discovered;
    case IconRole: return device.icon;
    case RssiRole: return device.rssi;
    case BatteryPercentRole: return device.batteryPercent;
    case Qt::DisplayRole: return device.name;
    default: return {};
    }
}

QHash<int, QByteArray> BluetoothDeviceModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {ObjectPathRole, "objectPath"},
        {AddressRole, "address"},
        {NameRole, "name"},
        {PairedRole, "paired"},
        {TrustedRole, "trusted"},
        {ConnectedRole, "connected"},
        {DiscoveredRole, "discovered"},
        {IconRole, "icon"},
        {RssiRole, "rssi"},
        {BatteryPercentRole, "batteryPercent"},
    };
}

void BluetoothDeviceModel::replace(QVector<BluetoothDevice> devices)
{
    std::sort(devices.begin(), devices.end(), [](const BluetoothDevice &left,
                                                 const BluetoothDevice &right) {
        const auto leftPriority = std::tuple{left.connected, left.paired, left.discovered};
        const auto rightPriority = std::tuple{right.connected, right.paired, right.discovered};
        if (leftPriority != rightPriority)
            return leftPriority > rightPriority;
        const QString leftName = left.name.isEmpty() ? left.address : left.name;
        const QString rightName = right.name.isEmpty() ? right.address : right.name;
        const int nameCompare = QString::compare(leftName, rightName, Qt::CaseInsensitive);
        return nameCompare == 0 ? left.address < right.address : nameCompare < 0;
    });
    if (devices == m_devices)
        return;
    beginResetModel();
    m_devices = std::move(devices);
    endResetModel();
}

} // namespace Astrea::System
