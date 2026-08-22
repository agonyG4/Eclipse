#include "statusnotifier/StatusNotifierItemProxy.hpp"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>

namespace Astrea::StatusNotifier {
namespace {

constexpr QLatin1StringView kFreedesktopItem("org.freedesktop.StatusNotifierItem");
constexpr QLatin1StringView kKdeItem("org.kde.StatusNotifierItem");

QVariant unwrap(const QVariant &value)
{
    if (value.canConvert<QDBusVariant>())
        return value.value<QDBusVariant>().variant();
    return value;
}

QList<PixmapData> parsePixmaps(const QVariant &value)
{
    QList<PixmapData> result;
    const QVariantList list = unwrap(value).toList();
    for (const QVariant &entry : list) {
        const QVariantList tuple = unwrap(entry).toList();
        if (tuple.size() < 3)
            continue;
        PixmapData pixmap;
        pixmap.width = tuple.at(0).toInt();
        pixmap.height = tuple.at(1).toInt();
        pixmap.argb32Network = unwrap(tuple.at(2)).toByteArray();
        result.append(pixmap);
    }
    return result;
}

} // namespace

StatusNotifierItemProxy::StatusNotifierItemProxy(const ItemAddress &address, quint64 generation,
                                                 QObject *parent)
    : QObject(parent), m_address(address), m_generation(generation)
{
    m_snapshot.address = address;
    m_snapshot.generation = generation;
    m_snapshot.category = QStringLiteral("ApplicationStatus");
}

void StatusNotifierItemProxy::start()
{
    if (m_started || !m_address.isValid())
        return;
    m_started = true;
    m_interfaceName = QString::fromLatin1(kFreedesktopItem);
    connectSignals(m_interfaceName);
    refresh(m_interfaceName, true);
}

void StatusNotifierItemProxy::stop()
{
    if (!m_started)
        return;
    m_started = false;
    ++m_requestGeneration;
    QObject::disconnect(this, nullptr, this, nullptr);
}

void StatusNotifierItemProxy::refresh(const QString &interfaceName, bool allowFallback)
{
    if (!m_started)
        return;
    QDBusInterface properties(m_address.service, m_address.objectPath,
                              QStringLiteral("org.freedesktop.DBus.Properties"),
                              QDBusConnection::sessionBus());
    const quint64 request = ++m_requestGeneration;
    auto *watcher = new QDBusPendingCallWatcher(
        properties.asyncCall(QStringLiteral("GetAll"), interfaceName), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, interfaceName, allowFallback, request] {
        const QDBusMessage reply = watcher->reply();
        watcher->deleteLater();
        if (!m_started || request != m_requestGeneration || m_snapshot.generation != m_generation)
            return;
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
            if (allowFallback && interfaceName == QString::fromLatin1(kFreedesktopItem)) {
                m_interfaceName = QString::fromLatin1(kKdeItem);
                connectSignals(m_interfaceName);
                refresh(m_interfaceName, false);
            } else {
                emit actionFailed(m_address.key(), reply.errorMessage());
            }
            return;
        }
        QVariant value = reply.arguments().constFirst();
        if (value.canConvert<QDBusArgument>()) {
            // QDBusArgument conversion is handled by QDBus for a{sv}; a QVariantMap is
            // available on all Qt 6 versions used by the shell.
            value = value.value<QDBusArgument>().asVariant();
        }
        const QVariantMap properties = value.toMap();
        if (properties.isEmpty()) {
            emit actionFailed(m_address.key(), QStringLiteral("StatusNotifierItem has no properties"));
            return;
        }
        m_interfaceName = interfaceName;
        applyProperties(interfaceName, properties);
    });
}

void StatusNotifierItemProxy::applyProperties(const QString &interfaceName,
                                               const QVariantMap &properties)
{
    if (interfaceName != m_interfaceName && !m_interfaceName.isEmpty())
        return;
    const auto value = [&properties](const QString &key) { return unwrap(properties.value(key)); };
    m_snapshot.id = value(QStringLiteral("Id")).toString();
    m_snapshot.title = value(QStringLiteral("Title")).toString();
    m_snapshot.category = value(QStringLiteral("Category")).toString();
    if (m_snapshot.category.isEmpty())
        m_snapshot.category = QStringLiteral("ApplicationStatus");
    m_snapshot.status = itemStatusFromString(value(QStringLiteral("Status")).toString());
    m_snapshot.iconName = value(QStringLiteral("IconName")).toString();
    m_snapshot.attentionIconName = value(QStringLiteral("AttentionIconName")).toString();
    m_snapshot.overlayIconName = value(QStringLiteral("OverlayIconName")).toString();
    m_snapshot.iconThemePath = value(QStringLiteral("IconThemePath")).toString();
    m_snapshot.pixmaps = parsePixmaps(value(QStringLiteral("IconPixmap")));
    m_snapshot.attentionPixmaps = parsePixmaps(value(QStringLiteral("AttentionIconPixmap")));
    m_snapshot.overlayPixmaps = parsePixmaps(value(QStringLiteral("OverlayIconPixmap")));
    const QVariant tooltip = value(QStringLiteral("ToolTip"));
    if (tooltip.typeId() == QMetaType::QString) {
        m_snapshot.tooltipTitle = tooltip.toString();
    } else {
        const QVariantList tuple = tooltip.toList();
        if (tuple.size() >= 2) {
            m_snapshot.tooltipTitle = tuple.at(0).toString();
            m_snapshot.tooltipDescription = tuple.at(1).toString();
        }
    }
    m_snapshot.menuPath = value(QStringLiteral("Menu")).toString();
    m_snapshot.itemIsMenu = value(QStringLiteral("ItemIsMenu")).toBool();
    m_snapshot.ready = !m_snapshot.id.isEmpty() || !m_snapshot.title.isEmpty()
        || !m_snapshot.iconName.isEmpty() || !m_snapshot.pixmaps.isEmpty();
    m_snapshot.generation = m_generation;
    emitSnapshot();
    emit menuPathChanged(m_address.key(), m_snapshot.menuPath);
}

void StatusNotifierItemProxy::connectSignals(const QString &interfaceName)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(m_address.service, m_address.objectPath, interfaceName,
                QStringLiteral("NewTitle"), this, SLOT(onNewTitle()));
    bus.connect(m_address.service, m_address.objectPath, interfaceName,
                QStringLiteral("NewIcon"), this, SLOT(onNewIcon()));
    bus.connect(m_address.service, m_address.objectPath, interfaceName,
                QStringLiteral("NewAttentionIcon"), this, SLOT(onNewAttentionIcon()));
    bus.connect(m_address.service, m_address.objectPath, interfaceName,
                QStringLiteral("NewOverlayIcon"), this, SLOT(onNewOverlayIcon()));
    bus.connect(m_address.service, m_address.objectPath, interfaceName,
                QStringLiteral("NewToolTip"), this, SLOT(onNewToolTip()));
    bus.connect(m_address.service, m_address.objectPath, interfaceName,
                QStringLiteral("NewStatus"), this, SLOT(onNewStatus(QString)));
    bus.connect(m_address.service, m_address.objectPath,
                QStringLiteral("org.freedesktop.DBus.Properties"),
                QStringLiteral("PropertiesChanged"), this,
                SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
}

void StatusNotifierItemProxy::callAction(const QString &method, const QVariantList &arguments)
{
    if (!m_started || m_interfaceName.isEmpty())
        return;
    QDBusInterface item(m_address.service, m_address.objectPath, m_interfaceName,
                        QDBusConnection::sessionBus());
    const quint64 request = m_requestGeneration;
    auto *watcher = new QDBusPendingCallWatcher(item.asyncCallWithArgumentList(method, arguments), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, request] {
        const QDBusMessage reply = watcher->reply();
        watcher->deleteLater();
        if (request != m_requestGeneration || !m_started)
            return;
        if (reply.type() == QDBusMessage::ErrorMessage)
            emit actionFailed(m_address.key(), reply.errorMessage());
    });
}

void StatusNotifierItemProxy::activate(int x, int y)
{
    callAction(QStringLiteral("Activate"), {x, y});
}

void StatusNotifierItemProxy::secondaryActivate(int x, int y)
{
    callAction(QStringLiteral("SecondaryActivate"), {x, y});
}

void StatusNotifierItemProxy::scroll(int delta, const QString &orientation)
{
    callAction(QStringLiteral("Scroll"), {delta, orientation});
}

void StatusNotifierItemProxy::onPropertiesChanged(const QString &interfaceName,
                                                   const QVariantMap &changed,
                                                   const QStringList &invalidated)
{
    if (interfaceName != m_interfaceName)
        return;
    Q_UNUSED(invalidated)
    Q_UNUSED(changed)
    refresh(interfaceName, false);
}

void StatusNotifierItemProxy::onNewTitle() { refresh(m_interfaceName, false); }
void StatusNotifierItemProxy::onNewIcon() { refresh(m_interfaceName, false); }
void StatusNotifierItemProxy::onNewAttentionIcon() { refresh(m_interfaceName, false); }
void StatusNotifierItemProxy::onNewOverlayIcon() { refresh(m_interfaceName, false); }
void StatusNotifierItemProxy::onNewToolTip() { refresh(m_interfaceName, false); }

void StatusNotifierItemProxy::onNewStatus(const QString &status)
{
    m_snapshot.status = itemStatusFromString(status);
    emitSnapshot();
}

void StatusNotifierItemProxy::emitSnapshot()
{
    if (m_snapshot.generation != m_generation)
        return;
    emit snapshotChanged(m_snapshot);
}

} // namespace Astrea::StatusNotifier
