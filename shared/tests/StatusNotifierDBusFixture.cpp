#include "statusnotifier/DBusMenuModel.hpp"
#include "statusnotifier/StatusNotifierTypes.hpp"

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QTimer>

using namespace Astrea::StatusNotifier;

namespace {

class FixtureItem final : public QObject {
    Q_OBJECT

public:
    StatusNotifierPixmapList pixmaps{{1, 1, QByteArray::fromHex("ff3366cc")}};
    int activateCalls = 0;
    int secondaryCalls = 0;
    int contextCalls = 0;
    int scrollCalls = 0;
    int lastX = 0;
    int lastY = 0;
    int lastDelta = 0;
    QString lastOrientation;
    bool itemIsMenuValue = false;
};

class FixtureItemAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.StatusNotifierItem")
    Q_PROPERTY(QString Category READ category)
    Q_PROPERTY(QString Id READ id)
    Q_PROPERTY(QString Title READ title)
    Q_PROPERTY(QString Status READ status)
    Q_PROPERTY(QString IconName READ iconName)
    Q_PROPERTY(StatusNotifierPixmapList IconPixmap READ iconPixmap)
    Q_PROPERTY(QString AttentionIconName READ attentionIconName)
    Q_PROPERTY(StatusNotifierPixmapList AttentionIconPixmap READ attentionIconPixmap)
    Q_PROPERTY(QString OverlayIconName READ overlayIconName)
    Q_PROPERTY(StatusNotifierPixmapList OverlayIconPixmap READ overlayIconPixmap)
    Q_PROPERTY(StatusNotifierToolTip ToolTip READ toolTip)
    Q_PROPERTY(bool ItemIsMenu READ itemIsMenu)
    Q_PROPERTY(QDBusObjectPath Menu READ menu)
    Q_PROPERTY(int ActivateCalls READ activateCalls)
    Q_PROPERTY(int SecondaryActivateCalls READ secondaryActivateCalls)
    Q_PROPERTY(int ContextMenuCalls READ contextMenuCalls)
    Q_PROPERTY(int ScrollCalls READ scrollCalls)
    Q_PROPERTY(int LastX READ lastX)
    Q_PROPERTY(int LastY READ lastY)
    Q_PROPERTY(int LastDelta READ lastDelta)
    Q_PROPERTY(QString LastOrientation READ lastOrientation)

public:
    explicit FixtureItemAdaptor(FixtureItem *item)
        : QDBusAbstractAdaptor(item), m_item(item)
    {
    }

    QString category() const { return QStringLiteral("ApplicationStatus"); }
    QString id() const { return QStringLiteral("integration-item"); }
    QString title() const { return QStringLiteral("Integration tray item"); }
    QString status() const { return QStringLiteral("Active"); }
    QString iconName() const { return {}; }
    StatusNotifierPixmapList iconPixmap() const { return m_item->pixmaps; }
    QString attentionIconName() const { return {}; }
    StatusNotifierPixmapList attentionIconPixmap() const { return {}; }
    QString overlayIconName() const { return {}; }
    StatusNotifierPixmapList overlayIconPixmap() const { return {}; }
    StatusNotifierToolTip toolTip() const
    {
        return {QString(), {}, QStringLiteral("Exact tooltip title"),
                QStringLiteral("Exact tooltip body")};
    }
    bool itemIsMenu() const { return m_item->itemIsMenuValue; }
    QDBusObjectPath menu() const { return QDBusObjectPath(QStringLiteral("/org/test/Menu")); }
    int activateCalls() const { return m_item->activateCalls; }
    int secondaryActivateCalls() const { return m_item->secondaryCalls; }
    int contextMenuCalls() const { return m_item->contextCalls; }
    int scrollCalls() const { return m_item->scrollCalls; }
    int lastX() const { return m_item->lastX; }
    int lastY() const { return m_item->lastY; }
    int lastDelta() const { return m_item->lastDelta; }
    QString lastOrientation() const { return m_item->lastOrientation; }

public slots:
    void Activate(int x, int y) { ++m_item->activateCalls; m_item->lastX = x; m_item->lastY = y; }
    void SecondaryActivate(int x, int y)
    { ++m_item->secondaryCalls; m_item->lastX = x; m_item->lastY = y; }
    void ContextMenu(int x, int y)
    { ++m_item->contextCalls; m_item->lastX = x; m_item->lastY = y; }
    void Scroll(int delta, const QString &orientation)
    {
        ++m_item->scrollCalls;
        m_item->lastDelta = delta;
        m_item->lastOrientation = orientation;
    }

signals:
    void NewTitle();
    void NewIcon();
    void NewAttentionIcon();
    void NewOverlayIcon();
    void NewToolTip();
    void NewStatus(const QString &status);

private:
    FixtureItem *m_item = nullptr;
};

class FixtureMenu final : public QObject {
    Q_OBJECT

public:
    quint32 revision = 4;
    bool submenuUpdated = false;
    bool emptyMenu = false;
    bool lazySubmenu = false;
    int eventCount = 0;
    int aboutToShowCount = 0;
    int rootAboutToShowCount = 0;
    int lastAboutToShowNode = -1;

    DBusMenuLayoutNodeWire root() const
    {
        DBusMenuLayoutNodeWire root;
        root.id = 0;
        if (emptyMenu)
            return root;
        DBusMenuLayoutNodeWire tools;
        tools.id = 10;
        tools.properties = {{QStringLiteral("label"), submenuUpdated
                                  ? QStringLiteral("_Tools updated")
                                  : QStringLiteral("_Tools")},
                             {QStringLiteral("children-display"), QStringLiteral("submenu")}};
        DBusMenuLayoutNodeWire open;
        open.id = 11;
        open.properties = {{QStringLiteral("label"), submenuUpdated
                                ? QStringLiteral("Open updated")
                                : QStringLiteral("Open")},
                           {QStringLiteral("enabled"), true},
                           {QStringLiteral("toggle-type"), QStringLiteral("checkmark")},
                           {QStringLiteral("toggle-state"), 1}};
        DBusMenuLayoutNodeWire nested;
        nested.id = 20;
        nested.properties = {{QStringLiteral("label"), submenuUpdated
                                  ? QStringLiteral("More updated")
                                  : QStringLiteral("More")},
                             {QStringLiteral("children-display"), QStringLiteral("submenu")}};
        DBusMenuLayoutNodeWire leaf;
        leaf.id = 30;
        leaf.properties = {{QStringLiteral("label"), QStringLiteral("Leaf")},
                           {QStringLiteral("enabled"), true}};
        nested.children = {leaf};
        tools.children = lazySubmenu ? QList<DBusMenuLayoutNodeWire>{}
                                     : QList<DBusMenuLayoutNodeWire>{open, nested};
        DBusMenuLayoutNodeWire separator;
        separator.id = 12;
        separator.properties = {{QStringLiteral("type"), QStringLiteral("separator")}};
        root.children = {tools, separator};
        return root;
    }

signals:
    void layoutUpdated(quint32 revision, int parentId);
    void itemsPropertiesUpdated(const QList<DBusMenuPropertyUpdate> &updated,
                                const QList<DBusMenuRemovedProperties> &removed);
};

class FixtureMenuAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.dbusmenu")
    Q_PROPERTY(int EventCount READ eventCount)
    Q_PROPERTY(int AboutToShowCount READ aboutToShowCount)
    Q_PROPERTY(int RootAboutToShowCount READ rootAboutToShowCount)
    Q_PROPERTY(int LastAboutToShowNode READ lastAboutToShowNode)

public:
    explicit FixtureMenuAdaptor(FixtureMenu *menu)
        : QDBusAbstractAdaptor(menu), m_menu(menu)
    {
        connect(m_menu, &FixtureMenu::layoutUpdated, this, &FixtureMenuAdaptor::LayoutUpdated);
        connect(m_menu, &FixtureMenu::itemsPropertiesUpdated, this,
                &FixtureMenuAdaptor::ItemsPropertiesUpdated);
    }

    int eventCount() const { return m_menu->eventCount; }
    int aboutToShowCount() const { return m_menu->aboutToShowCount; }
    int rootAboutToShowCount() const { return m_menu->rootAboutToShowCount; }
    int lastAboutToShowNode() const { return m_menu->lastAboutToShowNode; }

public slots:
    DBusMenuLayoutReply GetLayout(int parentId, int, const QStringList &)
    {
        const DBusMenuLayoutNodeWire fullRoot = m_menu->root();
        if (parentId == 0)
            return {m_menu->revision, fullRoot};
        if (parentId == 10) {
            for (const auto &node : fullRoot.children) {
                if (node.id == parentId)
                    return {m_menu->revision, node};
            }
        }
        if (parentId == 20) {
            for (const auto &node : fullRoot.children) {
                for (const auto &child : node.children) {
                    if (child.id == parentId)
                        return {m_menu->revision, child};
                }
            }
        }
        DBusMenuLayoutNodeWire unknown;
        unknown.id = parentId;
        return {m_menu->revision, unknown};
    }
    bool AboutToShow(int nodeId)
    {
        ++m_menu->aboutToShowCount;
        m_menu->lastAboutToShowNode = nodeId;
        if (nodeId == 0)
            ++m_menu->rootAboutToShowCount;
        if (nodeId == 10) {
            m_menu->lazySubmenu = false;
            m_menu->submenuUpdated = true;
            ++m_menu->revision;
        }
        if (nodeId == 20) {
            m_menu->submenuUpdated = true;
            ++m_menu->revision;
        }
        return nodeId == 10 || nodeId == 20;
    }
    void Event(int, const QString &, const QDBusVariant &, quint32) { ++m_menu->eventCount; }
    void SetSubmenuUpdated(bool updated)
    {
        m_menu->submenuUpdated = updated;
        ++m_menu->revision;
        emit m_menu->layoutUpdated(m_menu->revision, 0);
    }
    void SetEmptyMenu(bool empty)
    {
        m_menu->emptyMenu = empty;
        ++m_menu->revision;
        emit m_menu->layoutUpdated(m_menu->revision, 0);
    }
    void EmitPropertyUpdate(const QString &label)
    {
        DBusMenuPropertyUpdate update;
        update.id = 11;
        update.properties = {{QStringLiteral("label"), label}};
        emit m_menu->itemsPropertiesUpdated({update}, {});
    }

    void EmitRemovedProperty()
    {
        DBusMenuRemovedProperties removed;
        removed.id = 11;
        removed.properties = {QStringLiteral("label")};
        emit m_menu->itemsPropertiesUpdated({}, {removed});
    }

signals:
    void LayoutUpdated(quint32 revision, int parentId);
    void ItemsPropertiesUpdated(const QList<DBusMenuPropertyUpdate> &updated,
                                const QList<DBusMenuRemovedProperties> &removed);

private:
    FixtureMenu *m_menu = nullptr;
};

class FixtureControl final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.astrea.tests.StatusNotifierFixture")
    Q_PROPERTY(bool Ready READ ready CONSTANT)
    Q_PROPERTY(QString LastError READ lastError NOTIFY lastErrorChanged)

public:
    FixtureControl(FixtureItem *item, FixtureMenu *menu, QObject *parent = nullptr)
        : QObject(parent), m_item(item), m_menu(menu)
    {
    }

    bool ready() const { return true; }
    QString lastError() const { return m_lastError; }

public slots:
    bool RegisterServiceOnly() { return sendRegistration(QStringLiteral("org.astrea.tests.StatusNotifierFixtureItem")); }
    bool RegisterPathOnly() { return sendRegistration(QStringLiteral("/org/test/Tray")); }
    bool UnregisterServiceOnly() { return sendUnregistration(QStringLiteral("org.astrea.tests.StatusNotifierFixtureItem")); }
    bool UnregisterPathOnly() { return sendUnregistration(QStringLiteral("/org/test/Tray")); }
    void SetItemIsMenu(bool value) { m_item->itemIsMenuValue = value; }
    void SetEmptyMenu(bool value)
    {
        m_menu->emptyMenu = value;
        ++m_menu->revision;
        emit m_menu->layoutUpdated(m_menu->revision, 0);
    }
    void SetLazySubmenu(bool value)
    {
        m_menu->lazySubmenu = value;
        ++m_menu->revision;
        emit m_menu->layoutUpdated(m_menu->revision, 0);
    }
    void ResetCounters()
    {
        m_item->activateCalls = 0;
        m_item->secondaryCalls = 0;
        m_item->contextCalls = 0;
        m_item->scrollCalls = 0;
        m_menu->eventCount = 0;
        m_menu->aboutToShowCount = 0;
        m_menu->rootAboutToShowCount = 0;
    }
    void Exit() { QTimer::singleShot(0, qApp, &QCoreApplication::quit); }
    QString Status() const
    { return QStringLiteral("calls=%1 replyType=%2 operation=%3 error=%4")
          .arg(m_callCount).arg(m_lastReplyType).arg(m_lastOperation).arg(m_lastError); }

signals:
    void lastErrorChanged();

private:
    bool sendRegistration(const QString &registration)
    {
        return sendWatcherCall(QStringLiteral("RegisterStatusNotifierItem"), registration);
    }

    bool sendUnregistration(const QString &registration)
    {
        return sendWatcherCall(QStringLiteral("UnregisterStatusNotifierItem"), registration);
    }

    bool sendWatcherCall(const QString &method, const QString &registration)
    {
        ++m_callCount;
        QTimer::singleShot(0, this, [this, method, registration] {
            m_lastOperation = QStringLiteral("sending %1 registration=%2 owner=%3")
                .arg(method, registration, QDBusConnection::sessionBus().baseService());
            const QDBusMessage call = QDBusMessage::createMethodCall(
                QStringLiteral("org.freedesktop.StatusNotifierWatcher"),
                QStringLiteral("/StatusNotifierWatcher"),
                QStringLiteral("org.freedesktop.StatusNotifierWatcher"), method);
            QDBusMessage withArguments = call;
            withArguments << registration;
            auto *watcher = new QDBusPendingCallWatcher(
                QDBusConnection::sessionBus().asyncCall(withArguments), this);
            connect(watcher, &QDBusPendingCallWatcher::finished, this,
                    [this, watcher] {
                const QDBusMessage reply = watcher->reply();
                m_lastReplyType = static_cast<int>(reply.type());
                m_lastError = reply.errorMessage();
            if (reply.type() == QDBusMessage::ErrorMessage)
                emit lastErrorChanged();
                watcher->deleteLater();
            });
        });
        return true;
    }

    FixtureItem *m_item = nullptr;
    FixtureMenu *m_menu = nullptr;
    QString m_lastError;
    int m_callCount = 0;
    int m_lastReplyType = -1;
    QString m_lastOperation;
};

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    registerStatusNotifierDBusMetaTypes();
    registerDBusMenuMetaTypes();
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.registerService(QStringLiteral("org.astrea.tests.StatusNotifierFixture"))
            != QDBusConnectionInterface::ServiceRegistered
        || bus.registerService(QStringLiteral("org.astrea.tests.StatusNotifierFixtureItem"))
            != QDBusConnectionInterface::ServiceRegistered) {
        return 2;
    }
    FixtureItem item;
    FixtureItemAdaptor itemAdaptor(&item);
    FixtureMenu menu;
    FixtureMenuAdaptor menuAdaptor(&menu);
    FixtureControl control(&item, &menu);
    if (!bus.registerObject(QStringLiteral("/StatusNotifierItem"), &item,
                            QDBusConnection::ExportAdaptors)
        || !bus.registerObject(QStringLiteral("/org/test/Tray"), &item,
                            QDBusConnection::ExportAdaptors)
        || !bus.registerObject(QStringLiteral("/org/test/Menu"), &menu,
                               QDBusConnection::ExportAdaptors)
        || !bus.registerObject(QStringLiteral("/org/astrea/tests/StatusNotifierFixture"),
                               &control, QDBusConnection::ExportAllSlots
                                   | QDBusConnection::ExportAllProperties)) {
        return 2;
    }
    return app.exec();
}

#include "StatusNotifierDBusFixture.moc"
