#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

namespace Astrea::System {

struct WifiNetwork {
    QString ssid;
    int strength = 0;
    bool active = false;
    bool secured = false;
    int frequencyMHz = 0;
    QString bssid;

    friend bool operator==(const WifiNetwork &, const WifiNetwork &) = default;
};

class WifiNetworkModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        SsidRole = Qt::UserRole + 1,
        StrengthRole,
        ActiveRole,
        SecuredRole,
        FrequencyRole,
        BssidRole,
    };
    Q_ENUM(Role)

    explicit WifiNetworkModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void replace(QVector<WifiNetwork> networks);

private:
    QVector<WifiNetwork> m_networks;
};

} // namespace Astrea::System

Q_DECLARE_METATYPE(Astrea::System::WifiNetwork)
