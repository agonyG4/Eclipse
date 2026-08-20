#pragma once

#include "system/bluetooth/BluezBackend.hpp"

namespace Astrea::System {

class BluezObjectStore final {
public:
    void clear();
    void replace(BluezManagedObjects objects);
    void interfacesAdded(const QDBusObjectPath &path, const BluezDBusInterfaces &interfaces);
    void interfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces);
    bool propertiesChanged(const QDBusObjectPath &path, const QString &interfaceName,
                           const QVariantMap &changed);
    bool replaceInterface(const QDBusObjectPath &path, const QString &interfaceName,
                          QVariantMap properties);

    const BluezManagedObjects &objects() const { return m_objects; }

private:
    BluezManagedObjects m_objects;
};

} // namespace Astrea::System
