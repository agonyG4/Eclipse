#include "statusnotifier/StatusNotifierItemProxy.hpp"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>

namespace Astrea::StatusNotifier {
namespace {

constexpr QLatin1StringView kFreedesktopItem("org.freedesktop.StatusNotifierItem");
constexpr QLatin1StringView kKdeItem("org.kde.StatusNotifierItem");

QVariant unwrap(const QVariant &value)
{
    if (value.canConvert<QDBusArgument>())
        return value;
    if (value.canConvert<QDBusVariant>())
        return value.value<QDBusVariant>().variant();
    if (value.canConvert<QDBusObjectPath>())
        return value.value<QDBusObjectPath>().path();
    return value;
}

QList<PixmapData> parsePixmaps(const QVariant &input)
{
    const QVariant value = unwrap(input);
    QList<PixmapData> result;
    if (value.canConvert<QDBusArgument>()) {
        const QDBusArgument argument = value.value<QDBusArgument>();
        const QString signature = argument.currentSignature();
        if (signature.startsWith(QLatin1StringView("a("))) {
            argument.beginArray();
            while (!argument.atEnd()) {
                if (!argument.currentSignature().startsWith(QLatin1StringView("(i")))
                    break;
                StatusNotifierPixmap entry;
                argument >> entry;
                result.append({entry.width, entry.height, entry.bytes});
            }
            argument.endArray();
            return result;
        }
        if (signature.startsWith(QLatin1StringView("(i"))) {
            StatusNotifierPixmap entry;
            argument >> entry;
            result.append({entry.width, entry.height, entry.bytes});
            return result;
        }
    }
    if (value.canConvert<StatusNotifierPixmapList>()) {
        const StatusNotifierPixmapList typed = value.value<StatusNotifierPixmapList>();
        for (const StatusNotifierPixmap &entry : typed)
            result.append({entry.width, entry.height, entry.bytes});
        return result;
    }
    const QVariantList list = value.toList();
    for (const QVariant &entry : list) {
        const QVariant unwrappedEntry = unwrap(entry);
        if (unwrappedEntry.canConvert<QDBusArgument>()) {
            const QDBusArgument argument = unwrappedEntry.value<QDBusArgument>();
            if (argument.currentSignature().startsWith(QLatin1StringView("(i"))) {
                StatusNotifierPixmap pixmap;
                argument >> pixmap;
                result.append({pixmap.width, pixmap.height, pixmap.bytes});
            }
            continue;
        }
        const QVariantList tuple = unwrappedEntry.toList();
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

bool parseTooltip(const QVariant &input, QString *title, QString *description)
{
    const QVariant value = unwrap(input);
    if (value.canConvert<StatusNotifierToolTip>()) {
        const auto tooltip = value.value<StatusNotifierToolTip>();
        *title = tooltip.title;
        *description = tooltip.description;
        return true;
    }
    if (value.canConvert<QDBusArgument>()) {
        StatusNotifierToolTip tooltip;
        const QDBusArgument argument = value.value<QDBusArgument>();
        argument >> tooltip;
        *title = tooltip.title;
        *description = tooltip.description;
        return true;
    }
    const QVariantList tuple = value.toList();
    if (tuple.size() >= 4) {
        *title = unwrap(tuple.at(2)).toString();
        *description = unwrap(tuple.at(3)).toString();
        return true;
    }
    if (value.typeId() == QMetaType::QString) {
        *title = value.toString();
        return true;
    }
    return false;
}

} // namespace

StatusNotifierItemProxy::StatusNotifierItemProxy(const ItemAddress &address, quint64 generation,
                                                 QObject *parent)
    : QObject(parent), m_address(address), m_generation(generation)
{
    registerStatusNotifierDBusMetaTypes();
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
    disconnectSignals();
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
            const QDBusArgument argument = value.value<QDBusArgument>();
            QVariantMap decoded;
            argument >> decoded;
            value = decoded.isEmpty() ? argument.asVariant() : QVariant(decoded);
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
    m_snapshot.tooltipTitle.clear();
    m_snapshot.tooltipDescription.clear();
    parseTooltip(value(QStringLiteral("ToolTip")), &m_snapshot.tooltipTitle,
                 &m_snapshot.tooltipDescription);
    m_snapshot.menuPath = value(QStringLiteral("Menu")).toString();
    if (m_snapshot.menuPath == QStringLiteral("/"))
        m_snapshot.menuPath.clear();
    m_snapshot.itemIsMenu = value(QStringLiteral("ItemIsMenu")).toBool();
    m_snapshot.ready = true;
    m_snapshot.generation = m_generation;
    emitSnapshot();
}

void StatusNotifierItemProxy::connectSignals(const QString &interfaceName)
{
    if (interfaceName == m_connectedInterface)
        return;
    disconnectSignals();
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
    m_connectedInterface = interfaceName;
}

void StatusNotifierItemProxy::disconnectSignals()
{
    if (m_connectedInterface.isEmpty())
        return;
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QStringList signalNames{
        QStringLiteral("NewTitle"), QStringLiteral("NewIcon"),
        QStringLiteral("NewAttentionIcon"), QStringLiteral("NewOverlayIcon"),
        QStringLiteral("NewToolTip"), QStringLiteral("NewStatus")};
    for (const QString &signalName : signalNames) {
        bus.disconnect(m_address.service, m_address.objectPath, m_connectedInterface, signalName,
                       this, signalName == QStringLiteral("NewStatus")
                           ? SLOT(onNewStatus(QString))
                           : signalName == QStringLiteral("NewTitle") ? SLOT(onNewTitle())
                           : signalName == QStringLiteral("NewIcon") ? SLOT(onNewIcon())
                           : signalName == QStringLiteral("NewAttentionIcon")
                               ? SLOT(onNewAttentionIcon())
                           : signalName == QStringLiteral("NewOverlayIcon")
                               ? SLOT(onNewOverlayIcon())
                           : SLOT(onNewToolTip()));
    }
    bus.disconnect(m_address.service, m_address.objectPath,
                   QStringLiteral("org.freedesktop.DBus.Properties"),
                   QStringLiteral("PropertiesChanged"), this,
                   SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
    m_connectedInterface.clear();
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

void StatusNotifierItemProxy::contextMenu(int x, int y)
{
    callAction(QStringLiteral("ContextMenu"), {x, y});
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
    if (!m_started)
        return;
    m_snapshot.status = itemStatusFromString(status);
    emitSnapshot();
}

void StatusNotifierItemProxy::emitSnapshot()
{
    if (!m_started || m_snapshot.generation != m_generation)
        return;
    emit snapshotChanged(m_snapshot);
}

} // namespace Astrea::StatusNotifier
