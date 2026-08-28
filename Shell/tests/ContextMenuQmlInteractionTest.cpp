#include "core/ContextMenuController.hpp"
#include "core/ContextMenuModel.hpp"
#include "statusnotifier/DBusMenuModel.hpp"

#include <QAbstractListModel>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTest>

#include <utility>

using Astrea::Shell::ContextMenuController;
using Astrea::Shell::ContextMenuModel;
using Astrea::Shell::ContextMenuTarget;

class FakeTrayItemModel final : public QAbstractListModel {
    Q_OBJECT

public:
    struct Item {
        QString key;
        QString title;
        bool hasMenu = true;
        bool onlyMenu = false;
        bool ready = true;
    };

    enum Role {
        KeyRole = Qt::UserRole + 1,
        TitleRole,
        IconSourceRole,
        HasMenuRole,
        OnlyMenuRole,
        ReadyRole,
    };

    explicit FakeTrayItemModel(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
    }

    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : m_items.size();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
            return {};
        const Item &item = m_items.at(index.row());
        switch (role) {
        case KeyRole: return item.key;
        case TitleRole: return item.title;
        case IconSourceRole: return QString();
        case HasMenuRole: return item.hasMenu;
        case OnlyMenuRole: return item.onlyMenu;
        case ReadyRole: return item.ready;
        default: return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {
            {KeyRole, "key"},
            {TitleRole, "title"},
            {IconSourceRole, "iconSource"},
            {HasMenuRole, "hasMenu"},
            {OnlyMenuRole, "onlyMenu"},
            {ReadyRole, "ready"},
        };
    }

    void setItems(QVector<Item> items)
    {
        beginResetModel();
        m_items = std::move(items);
        endResetModel();
    }

private:
    QVector<Item> m_items;
};

class FakeTrayMenuService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *itemModel READ itemModel CONSTANT)
    Q_PROPERTY(quint64 presentationRevision READ presentationRevision
                   NOTIFY presentationRevisionChanged)

public:
    FakeTrayMenuService()
        : m_model(this)
        , m_modelA(this)
        , m_modelB(this)
        , m_itemModel(this)
    {
        m_itemModel.setItems({
            {.key = QStringLiteral("tray-test"), .title = QStringLiteral("Test tray")},
        });
    }

    Astrea::StatusNotifier::DBusMenuModel *model() { return &m_model; }
    Astrea::StatusNotifier::DBusMenuModel *modelForKey(const QString &key)
    {
        if (key == QStringLiteral("tray-a"))
            return &m_modelA;
        if (key == QStringLiteral("tray-b"))
            return &m_modelB;
        return &m_model;
    }
    QAbstractItemModel *itemModel() { return &m_itemModel; }
    quint64 presentationRevision() const { return m_revision; }
    int prepareCount() const { return m_prepareCount; }
    int remoteContextMenuCount() const { return m_remoteContextMenuCount; }

    void setItems(QVector<FakeTrayItemModel::Item> items)
    {
        m_itemModel.setItems(std::move(items));
    }

    void setLocalMenuAvailable(bool available)
    {
        if (m_localMenuAvailable == available)
            return;
        m_localMenuAvailable = available;
        ++m_revision;
        emit presentationRevisionChanged();
    }

    Q_INVOKABLE bool hasMenuForItem(const QString &key) const
    {
        return key == QStringLiteral("tray-test")
            || key == QStringLiteral("tray-a")
            || key == QStringLiteral("tray-b");
    }

    Q_INVOKABLE bool hasUsableMenuForItem(const QString &key) const
    {
        return m_localMenuAvailable && hasMenuForItem(key);
    }

    Q_INVOKABLE QObject *menuModelForItem(const QString &key) const
    {
        return hasMenuForItem(key) ? const_cast<FakeTrayMenuService *>(this)->modelForKey(key)
                                   : nullptr;
    }

    Q_INVOKABLE int menuStateForItem(const QString &key) const
    {
        return hasMenuForItem(key) ? 3 : 0;
    }

    Q_INVOKABLE QString displayTitleForItem(const QString &key) const
    {
        if (key == QStringLiteral("tray-a"))
            return QStringLiteral("Tray A");
        if (key == QStringLiteral("tray-b"))
            return QStringLiteral("Tray B");
        return hasMenuForItem(key) ? QStringLiteral("Test tray") : QString();
    }

    Q_INVOKABLE QString iconSourceForItem(const QString &) const { return {}; }

    Q_INVOKABLE QString tooltipTitleForItem(const QString &) const { return {}; }

    Q_INVOKABLE void contextMenu(const QString &, int, int)
    {
        ++m_remoteContextMenuCount;
    }

    Q_INVOKABLE void prepareMenuForPresentation(const QString &key, int = 0)
    {
        if (!hasMenuForItem(key))
            return;
        ++m_prepareCount;
        ++m_revision;
        emit presentationRevisionChanged();
        emit menuContentChanged(key);
    }

    Q_INVOKABLE void aboutToShowMenu(const QString &key, int nodeId)
    {
        prepareMenuForPresentation(key, nodeId);
        emit aboutToShowRequested(nodeId);
    }

signals:
    void presentationRevisionChanged();
    void menuClientChanged(const QString &key);
    void menuContentChanged(const QString &key);
    void aboutToShowRequested(int nodeId);

private:
    Astrea::StatusNotifier::DBusMenuModel m_model;
    Astrea::StatusNotifier::DBusMenuModel m_modelA;
    Astrea::StatusNotifier::DBusMenuModel m_modelB;
    FakeTrayItemModel m_itemModel;
    quint64 m_revision = 0;
    int m_prepareCount = 0;
    int m_remoteContextMenuCount = 0;
    bool m_localMenuAvailable = true;
};

class FakeContextMenuController final : public QObject {
    Q_OBJECT

public:
    bool acceptPresent = true;
    int presentCount = 0;
    QString lastItemKey;
    QRect lastAnchor;

    Q_INVOKABLE bool presentTray(const QString &itemKey, int x, int y,
                                 int width, int height, int, const QString &)
    {
        ++presentCount;
        lastItemKey = itemKey;
        lastAnchor = QRect(x, y, width, height);
        return acceptPresent;
    }
};

namespace {

constexpr QLatin1StringView kViewUrl(
    "qrc:/qt/qml/Astrea/Shell/ContextMenu/qml/ContextMenuView.qml");
constexpr QLatin1StringView kOverlayUrl(
    "qrc:/qt/qml/Astrea/Shell/ContextMenu/qml/ContextMenuOverlaySurface.qml");

ContextMenuTarget desktopTarget()
{
    return {ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
            QStringLiteral("test-output")};
}

QQuickItem *createView(QQmlEngine &engine, QQuickWindow &window,
                       ContextMenuController &controller)
{
    QQmlComponent component(&engine, QUrl(kViewUrl));
    if (component.status() != QQmlComponent::Ready)
        return nullptr;
    auto *view = qobject_cast<QQuickItem *>(component.createWithInitialProperties({
        {QStringLiteral("menuModel"), QVariant::fromValue(controller.model())},
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("presentationGeneration"), controller.presentationGeneration()},
        {QStringLiteral("outputWidth"), 280},
        {QStringLiteral("outputHeight"), 180},
    }));
    if (!view)
        return nullptr;
    view->setParentItem(window.contentItem());
    view->setWidth(280);
    window.setWidth(280);
    window.setHeight(180);
    view->forceActiveFocus();
    window.show();
    window.requestActivate();
    QCoreApplication::processEvents();
    return view;
}

QQuickItem *loadedChild(QQuickItem *view)
{
    for (QObject *object : view->findChildren<QObject *>()) {
        const QVariant itemValue = object->property("item");
        if (!itemValue.canConvert<QObject *>())
            continue;
        if (auto *item = qobject_cast<QQuickItem *>(itemValue.value<QObject *>()))
            return item;
    }
    return nullptr;
}

QQuickItem *findVisualItem(QQuickItem *root, const QString &objectName)
{
    if (!root)
        return nullptr;
    if (root->objectName() == objectName)
        return root;
    for (QQuickItem *child : root->childItems()) {
        if (QQuickItem *match = findVisualItem(child, objectName))
            return match;
    }
    return nullptr;
}

QVector<QQuickItem *> findVisualItems(QQuickItem *root, const QString &objectName)
{
    QVector<QQuickItem *> matches;
    if (!root)
        return matches;
    if (root->objectName() == objectName)
        matches.append(root);
    for (QQuickItem *child : root->childItems())
        matches.append(findVisualItems(child, objectName));
    return matches;
}

} // namespace

class ContextMenuQmlInteractionTest final : public QObject {
    Q_OBJECT

private slots:
    void keyboardNavigationSkipsDisabledAndSeparators();
    void keyboardActivationDispatchesLeafAction();
    void leftRestoresParentFocusAndSelection();
    void scrolledSubmenuUsesVisibleRowGeometry();
    void outsidePressClosesButInsideDisabledRowDoesNotHitShield();
    void closingDisablesInputAndCompletesAfterAnimation();
    void globalOverlayKeepsTrayMenuLiveAndDispatchable();
    void trayPlacementUsesResolvedRenderedWidth();
    void trayPresentationUpdatesReusedRenderer();
    void trayRightArrowOnlyOpensSubmenus();
    void trayPathFallsBackWhenLocalPresentationIsUnavailableOrRejected();
    void trayAnchorUsesStatusSurfaceContentCoordinates();
};

void ContextMenuQmlInteractionTest::keyboardNavigationSkipsDisabledAndSeparators()
{
    ContextMenuController controller;
    QVERIFY(controller.present(desktopTarget(), {
        {.token = QStringLiteral("first"), .label = QStringLiteral("First")},
        {.kind = ContextMenuModel::NodeKind::Separator},
        {.token = QStringLiteral("disabled"), .label = QStringLiteral("Disabled"),
         .enabled = false},
        {.token = QStringLiteral("second"), .label = QStringLiteral("Second")},
        {.kind = ContextMenuModel::NodeKind::Submenu, .label = QStringLiteral("More"),
         .children = {{.token = QStringLiteral("child"), .label = QStringLiteral("Child")}}},
    }, [](const QString &) { return true; }));

    QQmlEngine engine;
    QQuickWindow window;
    auto *view = createView(engine, window, controller);
    QVERIFY(view);
    QTRY_COMPARE(view->property("activeIndex").toInt(), 0);

    QTest::keyClick(&window, Qt::Key_Down);
    QCOMPARE(view->property("activeIndex").toInt(), 3);
    QTest::keyClick(&window, Qt::Key_Home);
    QCOMPARE(view->property("activeIndex").toInt(), 0);
    QTest::keyClick(&window, Qt::Key_End);
    QCOMPARE(view->property("activeIndex").toInt(), 4);
    delete view;
}

void ContextMenuQmlInteractionTest::keyboardActivationDispatchesLeafAction()
{
    ContextMenuController controller;
    int activations = 0;
    QVERIFY(controller.present(desktopTarget(), {
        {.token = QStringLiteral("action"), .label = QStringLiteral("Action")},
    }, [&](const QString &token) {
        ++activations;
        return token == QStringLiteral("action");
    }));

    QQmlEngine engine;
    QQuickWindow window;
    auto *view = createView(engine, window, controller);
    QVERIFY(view);
    QTest::keyClick(&window, Qt::Key_Space);
    QCOMPARE(activations, 1);
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closing);
    delete view;
}

void ContextMenuQmlInteractionTest::leftRestoresParentFocusAndSelection()
{
    ContextMenuController controller;
    QVERIFY(controller.present(desktopTarget(), {
        {.token = QStringLiteral("first"), .label = QStringLiteral("First")},
        {.kind = ContextMenuModel::NodeKind::Submenu, .label = QStringLiteral("More"),
         .children = {{.token = QStringLiteral("child"), .label = QStringLiteral("Child")}}},
        {.token = QStringLiteral("last"), .label = QStringLiteral("Last")},
    }, [](const QString &) { return true; }));

    QQmlEngine engine;
    QQuickWindow window;
    auto *view = createView(engine, window, controller);
    QVERIFY(view);
    QTest::keyClick(&window, Qt::Key_Down);
    QCOMPARE(view->property("activeIndex").toInt(), 1);
    QTest::keyClick(&window, Qt::Key_Right);
    QQuickItem *child = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((child = loadedChild(view)) != nullptr, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(window.activeFocusItem() == child, 1000);

    QTest::keyClick(&window, Qt::Key_Left);
    QTRY_VERIFY_WITH_TIMEOUT(window.activeFocusItem() == view, 1000);
    QCOMPARE(view->property("activeIndex").toInt(), 1);
    QTest::keyClick(&window, Qt::Key_Down);
    QCOMPARE(view->property("activeIndex").toInt(), 2);
    delete view;
}

void ContextMenuQmlInteractionTest::scrolledSubmenuUsesVisibleRowGeometry()
{
    ContextMenuController controller;
    QVector<ContextMenuModel::NodeSpec> nodes;
    for (int i = 0; i < 20; ++i) {
        nodes.append({.token = QStringLiteral("action-%1").arg(i),
                      .label = QStringLiteral("Action %1").arg(i)});
    }
    nodes.append({.kind = ContextMenuModel::NodeKind::Submenu, .label = QStringLiteral("More"),
                  .children = {{.token = QStringLiteral("child"), .label = QStringLiteral("Child")}}});
    QVERIFY(controller.present(desktopTarget(), nodes, [](const QString &) { return true; }));
    QCOMPARE(controller.model()->rowCount(), 21);
    QCOMPARE(controller.model()->nextNavigable(-1, -1), 20);

    QQmlEngine engine;
    QQuickWindow window;
    auto *view = createView(engine, window, controller);
    QVERIFY(view);
    auto *list = findVisualItem(view, QStringLiteral("contextMenuList"));
    QVERIFY(list);
    QCOMPARE(list->property("count").toInt(), 21);
    QTest::mouseMove(&window, QPoint(window.width() - 1, window.height() - 1));
    QCoreApplication::processEvents();
    QTRY_COMPARE_WITH_TIMEOUT(view->property("activeIndex").toInt(), 0, 1000);
    QTest::keyClick(&window, Qt::Key_End);
    QTRY_COMPARE_WITH_TIMEOUT(view->property("activeIndex").toInt(), 20, 1000);
    const QRectF row = view->property("activeRowRectangle").toRectF();
    QVERIFY(row.height() > 0);
    QVERIFY(row.y() >= 0);
    QVERIFY(row.bottom() <= window.height());
    QTest::keyClick(&window, Qt::Key_Right);
    QTRY_VERIFY_WITH_TIMEOUT(loadedChild(view) != nullptr, 1000);
    delete view;
}

void ContextMenuQmlInteractionTest::outsidePressClosesButInsideDisabledRowDoesNotHitShield()
{
    ContextMenuController controller;
    QVERIFY(controller.present(desktopTarget(), {
        {.token = QStringLiteral("disabled"), .label = QStringLiteral("Disabled"),
         .enabled = false},
    }, [](const QString &) { return true; }));

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(kOverlayUrl));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties({
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("outputKey"), QStringLiteral("test-output")},
        {QStringLiteral("outputWidth"), 280},
        {QStringLiteral("outputHeight"), 180},
    }));
    QVERIFY(window);
    window->setWidth(280);
    window->setHeight(180);
    window->show();
    QCoreApplication::processEvents();
    const auto menuView = window->findChild<QObject *>(QStringLiteral("contextMenuView"));
    QVERIFY(menuView);
    const int insideX = menuView->property("x").toInt() + 20;
    const int insideY = menuView->property("y").toInt() + 20;
    QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(insideX, insideY));
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Open);
    QTest::mouseClick(window, Qt::RightButton, {}, QPoint(window->width() - 1,
                                                          window->height() - 1));
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closing);
    controller.completeClose();
    delete window;
}

void ContextMenuQmlInteractionTest::closingDisablesInputAndCompletesAfterAnimation()
{
    ContextMenuController controller;
    QVERIFY(controller.present(desktopTarget(), {
        {.token = QStringLiteral("settings"), .label = QStringLiteral("Settings")},
    }, [](const QString &) { return true; }));

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(kOverlayUrl));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties({
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("outputKey"), QStringLiteral("test-output")},
        {QStringLiteral("outputWidth"), 280},
        {QStringLiteral("outputHeight"), 180},
    }));
    QVERIFY(window);
    window->setWidth(280);
    window->setHeight(180);
    window->show();
    QObject *menu = window->findChild<QObject *>(QStringLiteral("contextMenuView"));
    QVERIFY(menu);
    QTRY_VERIFY_WITH_TIMEOUT(menu->property("opacity").toReal() > 0.99, 1000);

    controller.close();
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closing);
    QVERIFY(!menu->property("enabled").toBool());
    QTRY_COMPARE_WITH_TIMEOUT(controller.lifecycle(), ContextMenuController::Lifecycle::Closed, 1000);
    delete window;
}

void ContextMenuQmlInteractionTest::globalOverlayKeepsTrayMenuLiveAndDispatchable()
{
    FakeTrayMenuService service;
    Astrea::StatusNotifier::DBusMenuNode root;
    root.children = {
        {.id = 7, .label = QStringLiteral("Remote action"), .enabled = true, .visible = true},
        {.id = 8, .label = QStringLiteral("Remote submenu"), .childrenDisplay = QStringLiteral("submenu"),
         .enabled = true, .visible = true,
         .children = {{.id = 9, .label = QStringLiteral("Nested submenu"),
                       .children = {{.id = 10, .label = QStringLiteral("Deep action")}}}}},
    };
    service.model()->setRoot(root);

    ContextMenuController controller;
    controller.setTrayService(&service);
    QSignalSpy activationSpy(service.model(),
                             &Astrea::StatusNotifier::DBusMenuModel::activateRequested);
    const ContextMenuTarget target{ContextMenuTarget::Kind::TrayItem,
                                   QStringLiteral("tray-test"), QStringLiteral("test-output")};
    QVERIFY(controller.present(target, {}, [&](const QString &token) {
        bool ok = false;
        const int nodeId = token.mid(QStringLiteral("tray.node.").size()).toInt(&ok);
        if (!ok)
            return false;
        service.model()->activate(nodeId);
        return true;
    }, [] { return true; }, [&](const QString &token) {
        bool ok = false;
        const int nodeId = token.mid(QStringLiteral("tray.node.").size()).toInt(&ok);
        const auto node = service.model()->nodeById(nodeId);
        return token.startsWith(QStringLiteral("tray.node.")) && ok && node.id == nodeId
            && node.visible && node.enabled && !node.separator && node.children.isEmpty();
    }));

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(kOverlayUrl));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties({
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("trayService"), QVariant::fromValue(&service)},
        {QStringLiteral("outputKey"), QStringLiteral("test-output")},
        {QStringLiteral("outputWidth"), 280},
        {QStringLiteral("outputHeight"), 180},
    }));
    QVERIFY(window);
    window->setWidth(280);
    window->setHeight(180);
    window->show();
    window->requestActivate();
    QObject *menu = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((menu = window->findChild<QObject *>(
                                  QStringLiteral("trayContextMenu"))) != nullptr, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(menu->property("menuModel").value<QObject *>(),
                              static_cast<QObject *>(service.model()), 1000);
    QVERIFY(service.prepareCount() > 0);
    QTRY_VERIFY_WITH_TIMEOUT(window->activeFocusItem() != nullptr, 1000);
    QCOMPARE(window->activeFocusItem(), qobject_cast<QQuickItem *>(menu));

    QTest::keyClick(window, Qt::Key_Down);
    QTest::keyClick(window, Qt::Key_Right);
    QObject *childMenu = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((childMenu = window->findChild<QObject *>(
                                  QStringLiteral("trayCascadeMenu"))) != nullptr, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->activeFocusItem(),
                              qobject_cast<QQuickItem *>(childMenu), 1000);
    QTest::keyClick(window, Qt::Key_Right);
    QTRY_VERIFY_WITH_TIMEOUT(window->findChildren<QObject *>(
                                 QStringLiteral("trayCascadeMenu")).size() == 2, 1000);
    const auto cascades = window->findChildren<QObject *>(QStringLiteral("trayCascadeMenu"));
    QObject *deepMenu = cascades.constLast();
    QTRY_COMPARE_WITH_TIMEOUT(window->activeFocusItem(),
                              qobject_cast<QQuickItem *>(deepMenu), 1000);
    QTest::keyClick(window, Qt::Key_Left);
    QTRY_COMPARE_WITH_TIMEOUT(window->activeFocusItem(),
                              qobject_cast<QQuickItem *>(childMenu), 1000);
    QTest::keyClick(window, Qt::Key_Left);
    QTRY_COMPARE_WITH_TIMEOUT(window->activeFocusItem(),
                              qobject_cast<QQuickItem *>(menu), 1000);
    QCOMPARE(menu->property("activeIndex").toInt(), 1);

    QTest::keyClick(window, Qt::Key_Down);
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_COMPARE_WITH_TIMEOUT(activationSpy.count(), 1, 1000);
    QCOMPARE(activationSpy.at(0).at(0).toInt(), 7);
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closing);
    controller.completeClose();
    delete window;
}

void ContextMenuQmlInteractionTest::trayPlacementUsesResolvedRenderedWidth()
{
    FakeTrayMenuService service;
    Astrea::StatusNotifier::DBusMenuNode action;
    action.id = 7;
    action.label = QStringLiteral("Remote action");
    service.model()->setNodes({action});

    ContextMenuController controller;
    const ContextMenuTarget target{ContextMenuTarget::Kind::TrayItem,
                                   QStringLiteral("tray-test"), QStringLiteral("test-output")};
    QVERIFY(controller.present(target, {}, [](const QString &) { return true; }));

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(kOverlayUrl));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties({
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("trayService"), QVariant::fromValue(&service)},
        {QStringLiteral("outputKey"), QStringLiteral("test-output")},
        {QStringLiteral("outputWidth"), 280},
        {QStringLiteral("outputHeight"), 180},
    }));
    QVERIFY(window);
    window->setWidth(280);
    window->setHeight(180);
    window->show();

    QObject *menu = nullptr;
    QObject *loader = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((menu = window->findChild<QObject *>(
                                  QStringLiteral("trayContextMenu"))) != nullptr, 1000);
    QTRY_VERIFY_WITH_TIMEOUT((loader = window->findChild<QObject *>(
                                  QStringLiteral("trayMenuLoader"))) != nullptr, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(menu->property("width").toReal()
                                      - loader->property("width").toReal()) < 0.01,
                             1000);
    QCOMPARE(qRound(menu->property("width").toReal()),
             qRound(menu->property("implicitWidth").toReal()));
    QCOMPARE(qRound(menu->property("width").toReal()),
             qRound(loader->property("width").toReal()));
    const QPoint expected = controller.menuPosition(
        280, 180, qRound(loader->property("width").toReal()),
        qRound(loader->property("height").toReal()));
    QCOMPARE(QPoint(loader->property("x").toInt(), loader->property("y").toInt()), expected);

    controller.completeClose();
    delete window;
}

void ContextMenuQmlInteractionTest::trayPresentationUpdatesReusedRenderer()
{
    FakeTrayMenuService service;
    service.modelForKey(QStringLiteral("tray-a"))->setNodes({
        {.id = 11, .label = QStringLiteral("Action A")},
    });
    service.modelForKey(QStringLiteral("tray-b"))->setNodes({
        {.id = 22, .label = QStringLiteral("Action B")},
    });

    ContextMenuController controller;
    controller.setTrayService(&service);
    QSignalSpy aActivations(service.modelForKey(QStringLiteral("tray-a")),
                             &Astrea::StatusNotifier::DBusMenuModel::activateRequested);
    QSignalSpy bActivations(service.modelForKey(QStringLiteral("tray-b")),
                             &Astrea::StatusNotifier::DBusMenuModel::activateRequested);
    const auto present = [&](const QString &key) {
        const ContextMenuTarget target{ContextMenuTarget::Kind::TrayItem, key,
                                       QStringLiteral("test-output")};
        return controller.present(target, {}, [&, key](const QString &token) {
            bool ok = false;
            const int nodeId = token.mid(QStringLiteral("tray.node.").size()).toInt(&ok);
            if (!ok)
                return false;
            service.modelForKey(key)->activate(nodeId);
            return true;
        }, [&, key] {
            return service.hasUsableMenuForItem(key);
        }, [&, key](const QString &token) {
            bool ok = false;
            const int nodeId = token.mid(QStringLiteral("tray.node.").size()).toInt(&ok);
            const auto node = service.modelForKey(key)->nodeById(nodeId);
            return token.startsWith(QStringLiteral("tray.node.")) && ok && node.id == nodeId
                && node.visible && node.enabled && !node.separator && node.children.isEmpty();
        });
    };

    QVERIFY(present(QStringLiteral("tray-a")));
    const quint64 generationA = controller.presentationGeneration();

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(kOverlayUrl));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties({
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("trayService"), QVariant::fromValue(&service)},
        {QStringLiteral("outputKey"), QStringLiteral("test-output")},
        {QStringLiteral("outputWidth"), 320},
        {QStringLiteral("outputHeight"), 200},
    }));
    QVERIFY(window);
    window->setWidth(320);
    window->setHeight(200);
    window->show();
    window->requestActivate();

    QObject *menu = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((menu = window->findChild<QObject *>(
                                  QStringLiteral("trayContextMenu"))) != nullptr, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(menu->property("contextKey").toString(),
                              QStringLiteral("tray-a"), 1000);
    QCOMPARE(menu->property("menuModel").value<QObject *>(),
             static_cast<QObject *>(service.modelForKey(QStringLiteral("tray-a"))));

    QVERIFY(present(QStringLiteral("tray-b")));
    const quint64 generationB = controller.presentationGeneration();
    QVERIFY(generationB > generationA);
    QTRY_COMPARE_WITH_TIMEOUT(menu->property("contextKey").toString(),
                              QStringLiteral("tray-b"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(menu->property("menuModel").value<QObject *>(),
                              static_cast<QObject *>(service.modelForKey(QStringLiteral("tray-b"))),
                              1000);
    QCOMPARE(window->findChild<QObject *>(QStringLiteral("trayContextMenu")), menu);
    QVERIFY(!controller.activate(generationA, QStringLiteral("tray.node.11")));
    QVERIFY(controller.activate(generationB, QStringLiteral("tray.node.22")));
    QCOMPARE(bActivations.count(), 1);

    controller.completeClose();
    QTRY_VERIFY_WITH_TIMEOUT(window->findChild<QObject *>(
                                 QStringLiteral("trayContextMenu")) == nullptr, 1000);
    QVERIFY(present(QStringLiteral("tray-a")));
    const quint64 generationA2 = controller.presentationGeneration();
    QTRY_VERIFY_WITH_TIMEOUT((menu = window->findChild<QObject *>(
                                  QStringLiteral("trayContextMenu"))) != nullptr, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(menu->property("contextKey").toString(),
                              QStringLiteral("tray-a"), 1000);
    QVERIFY(present(QStringLiteral("tray-b")));
    const quint64 generationB2 = controller.presentationGeneration();
    QVERIFY(present(QStringLiteral("tray-a")));
    const quint64 generationA3 = controller.presentationGeneration();
    QVERIFY(generationA3 > generationB2);
    QTRY_COMPARE_WITH_TIMEOUT(menu->property("contextKey").toString(),
                              QStringLiteral("tray-a"), 1000);
    QCOMPARE(menu->property("menuModel").value<QObject *>(),
             static_cast<QObject *>(service.modelForKey(QStringLiteral("tray-a"))));
    QVERIFY(!controller.activate(generationB2, QStringLiteral("tray.node.22")));
    QVERIFY(controller.activate(generationA3, QStringLiteral("tray.node.11")));
    QCOMPARE(aActivations.count(), 1);
    QVERIFY(generationA2 < generationB2);

    controller.completeClose();
    delete window;
}

void ContextMenuQmlInteractionTest::trayRightArrowOnlyOpensSubmenus()
{
    FakeTrayMenuService service;
    service.model()->setNodes({
        {.id = 7, .label = QStringLiteral("Leaf")},
        {.id = 8, .label = QStringLiteral("Submenu"),
         .children = {{.id = 9, .label = QStringLiteral("Nested leaf")}}},
    });

    ContextMenuController controller;
    controller.setTrayService(&service);
    int activations = 0;
    const ContextMenuTarget target{ContextMenuTarget::Kind::TrayItem,
                                   QStringLiteral("tray-test"), QStringLiteral("test-output")};
    QVERIFY(controller.present(target, {}, [&](const QString &token) {
        bool ok = false;
        const int nodeId = token.mid(QStringLiteral("tray.node.").size()).toInt(&ok);
        if (!ok)
            return false;
        ++activations;
        service.model()->activate(nodeId);
        return true;
    }, [] { return true; }, [&](const QString &token) {
        bool ok = false;
        const int nodeId = token.mid(QStringLiteral("tray.node.").size()).toInt(&ok);
        const auto node = service.model()->nodeById(nodeId);
        return token.startsWith(QStringLiteral("tray.node.")) && ok && node.id == nodeId
            && node.visible && node.enabled && !node.separator && node.children.isEmpty();
    }));

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(kOverlayUrl));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties({
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&controller)},
        {QStringLiteral("trayService"), QVariant::fromValue(&service)},
        {QStringLiteral("outputKey"), QStringLiteral("test-output")},
        {QStringLiteral("outputWidth"), 320},
        {QStringLiteral("outputHeight"), 200},
    }));
    QVERIFY(window);
    window->setWidth(320);
    window->setHeight(200);
    window->show();

    QObject *menu = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((menu = window->findChild<QObject *>(
                                  QStringLiteral("trayContextMenu"))) != nullptr, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->activeFocusItem(), qobject_cast<QQuickItem *>(menu), 1000);

    QTest::keyClick(window, Qt::Key_Right);
    QCOMPARE(activations, 0);
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Open);

    QTest::keyClick(window, Qt::Key_Down);
    QCOMPARE(menu->property("activeIndex").toInt(), 1);
    QTest::keyClick(window, Qt::Key_Right);
    QObject *child = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((child = window->findChild<QObject *>(
                                  QStringLiteral("trayCascadeMenu"))) != nullptr, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->activeFocusItem(), qobject_cast<QQuickItem *>(child), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(child->property("activeIndex").toInt(), 0, 1000);
    QTest::keyClick(window, Qt::Key_Right);
    QCOMPARE(window->activeFocusItem(), qobject_cast<QQuickItem *>(child));
    QCOMPARE(child->property("activeIndex").toInt(), 0);
    QCOMPARE(activations, 0);
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Open);
    QTest::keyClick(window, Qt::Key_Enter);
    QCOMPARE(activations, 1);
    QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closing);

    controller.completeClose();
    delete window;
}

void ContextMenuQmlInteractionTest::trayPathFallsBackWhenLocalPresentationIsUnavailableOrRejected()
{
    FakeTrayMenuService service;
    FakeContextMenuController contextMenuController;
    QQmlEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/Bar/qml/Tray.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *tray = qobject_cast<QQuickItem *>(component.createWithInitialProperties({
        {QStringLiteral("trayService"), QVariant::fromValue(&service)},
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&contextMenuController)},
        {QStringLiteral("outputWidth"), 640},
        {QStringLiteral("statusLeft"), 80},
        {QStringLiteral("statusTop"), 6},
    }));
    QVERIFY(tray);

    QQuickWindow window;
    window.resize(640, 80);
    tray->setParentItem(window.contentItem());
    tray->setX(40);
    tray->setY(20);
    window.show();
    QTRY_COMPARE_WITH_TIMEOUT(tray->property("itemCount").toInt(), 1, 1000);
    auto *surface = findVisualItem(tray, QStringLiteral("trayItemSurface"));
    QVERIFY(surface);
    const auto click = [&] {
        const QPointF point = surface->mapToItem(window.contentItem(),
                                                  QPointF(surface->width() / 2,
                                                          surface->height() / 2));
        QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier, point.toPoint());
        QCoreApplication::processEvents();
    };

    click();
    QCOMPARE(contextMenuController.presentCount, 1);
    QCOMPARE(service.remoteContextMenuCount(), 0);

    contextMenuController.acceptPresent = false;
    click();
    QCOMPARE(contextMenuController.presentCount, 2);
    QCOMPARE(service.remoteContextMenuCount(), 1);

    contextMenuController.acceptPresent = true;
    service.setLocalMenuAvailable(false);
    QCoreApplication::processEvents();
    click();
    QCOMPARE(contextMenuController.presentCount, 2);
    QCOMPARE(service.remoteContextMenuCount(), 2);

    delete tray;
}

void ContextMenuQmlInteractionTest::trayAnchorUsesStatusSurfaceContentCoordinates()
{
    FakeTrayMenuService service;
    service.setItems({
        {.key = QStringLiteral("tray-test"), .title = QStringLiteral("First")},
        {.key = QStringLiteral("tray-test"), .title = QStringLiteral("Last")},
    });
    FakeContextMenuController contextMenuController;
    QQmlEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral(
                                "qrc:/qt/qml/Astrea/Shell/Bar/qml/Tray.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *tray = qobject_cast<QQuickItem *>(component.createWithInitialProperties({
        {QStringLiteral("trayService"), QVariant::fromValue(&service)},
        {QStringLiteral("contextMenuController"), QVariant::fromValue(&contextMenuController)},
        {QStringLiteral("statusLeft"), 111},
        {QStringLiteral("statusTop"), 7},
    }));
    QVERIFY(tray);

    QQuickWindow window;
    window.resize(800, 120);
    auto *surfaceRoot = new QQuickItem(window.contentItem());
    surfaceRoot->setX(17);
    surfaceRoot->setY(9);
    surfaceRoot->setWidth(700);
    surfaceRoot->setHeight(40);
    auto *statusContainer = new QQuickItem(surfaceRoot);
    statusContainer->setX(31);
    statusContainer->setY(4);
    auto *contentRow = new QQuickItem(statusContainer);
    contentRow->setX(23);
    contentRow->setY(5);
    tray->setParentItem(contentRow);
    tray->setX(19);
    tray->setY(3);
    tray->setProperty("surfaceContentItem", QVariant::fromValue(surfaceRoot));
    window.show();
    QTRY_COMPARE_WITH_TIMEOUT(tray->property("itemCount").toInt(), 2, 1000);

    const auto trayItems = findVisualItems(tray, QStringLiteral("trayItem"));
    QCOMPARE(trayItems.size(), 2);
    for (QQuickItem *item : trayItems) {
        auto *surface = findVisualItem(item, QStringLiteral("trayItemSurface"));
        QVERIFY(surface);
        const QPointF point = surface->mapToItem(window.contentItem(),
                                                  QPointF(surface->width() / 2,
                                                          surface->height() / 2));
        QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier, point.toPoint());
        QCoreApplication::processEvents();

        const QPointF surfaceLocal = item->mapToItem(surfaceRoot, QPointF(0, 0));
        QCOMPARE(contextMenuController.lastAnchor.x(), qRound(111 + surfaceLocal.x()));
        QCOMPARE(contextMenuController.lastAnchor.y(), qRound(7 + surfaceLocal.y()));
    }

    delete tray;
    delete surfaceRoot;
}

QTEST_MAIN(ContextMenuQmlInteractionTest)
#include "ContextMenuQmlInteractionTest.moc"
