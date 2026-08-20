#include "system/bluetooth/BluezObjectStore.hpp"

namespace Astrea::System {

void BluezObjectStore::clear()
{
    m_objects.clear();
}

void BluezObjectStore::replace(BluezManagedObjects objects)
{
    m_objects = std::move(objects);
}

void BluezObjectStore::interfacesAdded(const QDBusObjectPath &path,
                                       const BluezDBusInterfaces &interfaces)
{
    BluezDBusInterfaces &object = m_objects[path];
    for (auto it = interfaces.cbegin(); it != interfaces.cend(); ++it)
        object.insert(it.key(), it.value());
}

void BluezObjectStore::interfacesRemoved(const QDBusObjectPath &path,
                                         const QStringList &interfaces)
{
    auto object = m_objects.find(path);
    if (object == m_objects.end())
        return;
    if (interfaces.isEmpty()) {
        m_objects.erase(object);
        return;
    }
    for (const QString &interfaceName : interfaces)
        object->remove(interfaceName);
    if (object->isEmpty())
        m_objects.erase(object);
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
    return true;
}

bool BluezObjectStore::replaceInterface(const QDBusObjectPath &path,
                                        const QString &interfaceName,
                                        QVariantMap properties)
{
    auto object = m_objects.find(path);
    if (object == m_objects.end() || !object->contains(interfaceName))
        return false;
    object->insert(interfaceName, std::move(properties));
    return true;
}

} // namespace Astrea::System
