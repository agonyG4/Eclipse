#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

namespace Astrea::System {

struct BluetoothDevice {
    QString id;
    QString objectPath;
    QString address;
    QString name;
    bool paired = false;
    bool trusted = false;
    bool connected = false;
    bool discovered = false;
    QString icon;
    int rssi = -1;
    int batteryPercent = -1;

    friend bool operator==(const BluetoothDevice &, const BluetoothDevice &) = default;
};

class BluetoothDeviceModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        ObjectPathRole,
        AddressRole,
        NameRole,
        PairedRole,
        TrustedRole,
        ConnectedRole,
        DiscoveredRole,
        IconRole,
        RssiRole,
        BatteryPercentRole,
    };
    Q_ENUM(Role)

    explicit BluetoothDeviceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void replace(QVector<BluetoothDevice> devices);

private:
    QVector<BluetoothDevice> m_devices;
};

} // namespace Astrea::System

Q_DECLARE_METATYPE(Astrea::System::BluetoothDevice)
