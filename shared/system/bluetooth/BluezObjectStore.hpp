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
    bool replaceInterfaceIfRevision(const QDBusObjectPath &path, const QString &interfaceName,
                                    quint64 expectedRevision, QVariantMap properties);
    quint64 interfaceRevision(const QDBusObjectPath &path, const QString &interfaceName) const;

    const BluezManagedObjects &objects() const { return m_objects; }

private:
    BluezManagedObjects m_objects;
    QMap<QDBusObjectPath, QMap<QString, quint64>> m_revisions;
    quint64 m_nextRevision = 0;
};

} // namespace Astrea::System
