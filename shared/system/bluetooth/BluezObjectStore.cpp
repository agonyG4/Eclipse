#include "system/bluetooth/BluezObjectStore.hpp"

namespace Astrea::System {

void BluezObjectStore::clear()
{
    m_objects.clear();
    m_revisions.clear();
}

void BluezObjectStore::replace(BluezManagedObjects objects)
{
    m_objects.clear();
    m_revisions.clear();
    for (auto object = objects.cbegin(); object != objects.cend(); ++object) {
        m_objects.insert(object.key(), object.value());
        auto &revisions = m_revisions[object.key()];
        for (auto interface = object.value().cbegin(); interface != object.value().cend();
             ++interface)
            revisions.insert(interface.key(), ++m_nextRevision);
    }
}

void BluezObjectStore::interfacesAdded(const QDBusObjectPath &path,
                                       const BluezDBusInterfaces &interfaces)
{
    BluezDBusInterfaces &object = m_objects[path];
    auto &revisions = m_revisions[path];
    for (auto it = interfaces.cbegin(); it != interfaces.cend(); ++it) {
        object.insert(it.key(), it.value());
        revisions.insert(it.key(), ++m_nextRevision);
    }
}

void BluezObjectStore::interfacesRemoved(const QDBusObjectPath &path,
                                         const QStringList &interfaces)
{
    auto object = m_objects.find(path);
    if (object == m_objects.end())
        return;
    if (interfaces.isEmpty()) {
        m_objects.erase(object);
        m_revisions.remove(path);
        return;
    }
    auto revisions = m_revisions.find(path);
    for (const QString &interfaceName : interfaces) {
        object->remove(interfaceName);
        if (revisions != m_revisions.end())
            revisions->remove(interfaceName);
    }
    if (object->isEmpty()) {
        m_objects.erase(object);
        m_revisions.remove(path);
    }
}

bool BluezObjectStore::propertiesChanged(const QDBusObjectPath &path,
                                         const QString &interfaceName,
                                         const QVariantMap &changed)
{
    auto object = m_objects.find(path);
    if (object == m_objects.end())
        return false;
    auto interface = object->find(interfaceName);
    if (interface == object->end())
        return false;
    for (auto it = changed.cbegin(); it != changed.cend(); ++it)
        interface->insert(it.key(), it.value());
    m_revisions[path][interfaceName] = ++m_nextRevision;
    return true;
}

bool BluezObjectStore::replaceInterface(const QDBusObjectPath &path,
                                        const QString &interfaceName,
                                        QVariantMap properties)
{
    return replaceInterfaceIfRevision(path, interfaceName, interfaceRevision(path, interfaceName),
                                      std::move(properties));
}

bool BluezObjectStore::replaceInterfaceIfRevision(const QDBusObjectPath &path,
                                                  const QString &interfaceName,
                                                  quint64 expectedRevision,
                                                  QVariantMap properties)
{
    auto object = m_objects.find(path);
    if (object == m_objects.end() || !object->contains(interfaceName)
        || interfaceRevision(path, interfaceName) != expectedRevision)
        return false;
    object->insert(interfaceName, std::move(properties));
    m_revisions[path][interfaceName] = ++m_nextRevision;
    return true;
}

quint64 BluezObjectStore::interfaceRevision(const QDBusObjectPath &path,
                                            const QString &interfaceName) const
{
    const auto object = m_revisions.constFind(path);
    if (object == m_revisions.cend())
        return 0;
    return object->value(interfaceName, 0);
}

} // namespace Astrea::System
