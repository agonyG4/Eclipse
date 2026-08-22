#include "statusnotifier/DBusMenuModel.hpp"
#include "statusnotifier/StatusNotifierTypes.hpp"

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusObjectPath>

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
    bool itemIsMenu() const { return false; }
    QDBusObjectPath menu() const { return QDBusObjectPath(QStringLiteral("/org/test/Menu")); }
    int activateCalls() const { return m_item->activateCalls; }
    int secondaryActivateCalls() const { return m_item->secondaryCalls; }
    int contextMenuCalls() const { return m_item->contextCalls; }
    int scrollCalls() const { return m_item->scrollCalls; }

public slots:
    void Activate(int, int) { ++m_item->activateCalls; }
    void SecondaryActivate(int, int) { ++m_item->secondaryCalls; }
    void ContextMenu(int, int) { ++m_item->contextCalls; }
    void Scroll(int, const QString &) { ++m_item->scrollCalls; }

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
    int eventCount = 0;
    int aboutToShowCount = 0;

    DBusMenuLayoutNodeWire root() const
    {
        DBusMenuLayoutNodeWire root;
        root.id = 0;
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
        tools.children = {open};
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

public slots:
    DBusMenuLayoutReply GetLayout(int, int, const QStringList &)
    {
        return {m_menu->revision, m_menu->root()};
    }
    bool AboutToShow(int nodeId)
    {
        ++m_menu->aboutToShowCount;
        if (nodeId == 10) {
            m_menu->submenuUpdated = true;
            ++m_menu->revision;
        }
        return nodeId == 10;
    }
    void Event(int, const QString &, const QVariant &, quint32) { ++m_menu->eventCount; }
    void SetSubmenuUpdated(bool updated)
    {
        m_menu->submenuUpdated = updated;
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

signals:
    void LayoutUpdated(quint32 revision, int parentId);
    void ItemsPropertiesUpdated(const QList<DBusMenuPropertyUpdate> &updated,
                                const QList<DBusMenuRemovedProperties> &removed);

private:
    FixtureMenu *m_menu = nullptr;
};

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    registerStatusNotifierDBusMetaTypes();
    registerDBusMenuMetaTypes();
    QDBusConnection bus = QDBusConnection::sessionBus();
    FixtureItem item;
    FixtureItemAdaptor itemAdaptor(&item);
    FixtureMenu menu;
    FixtureMenuAdaptor menuAdaptor(&menu);
    if (!bus.registerObject(QStringLiteral("/org/test/Tray"), &item,
                            QDBusConnection::ExportAdaptors)
        || !bus.registerObject(QStringLiteral("/org/test/Menu"), &menu,
                               QDBusConnection::ExportAdaptors)) {
        return 2;
    }

    const QDBusMessage registration = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.StatusNotifierWatcher"),
        QStringLiteral("/StatusNotifierWatcher"),
        QStringLiteral("org.freedesktop.StatusNotifierWatcher"),
        QStringLiteral("RegisterStatusNotifierItem"));
    QDBusMessage registrationWithArgs = registration;
    registrationWithArgs << QStringLiteral("/org/test/Tray");
    auto *registrationWatcher = new QDBusPendingCallWatcher(
        bus.asyncCall(registrationWithArgs), &app);
    QObject::connect(registrationWatcher, &QDBusPendingCallWatcher::finished,
                     &app, [registrationWatcher](QDBusPendingCallWatcher *) {
        const QDBusMessage reply = registrationWatcher->reply();
        if (reply.type() == QDBusMessage::ErrorMessage)
            QCoreApplication::exit(3);
        registrationWatcher->deleteLater();
    });
    return app.exec();
}

#include "StatusNotifierDBusFixture.moc"
