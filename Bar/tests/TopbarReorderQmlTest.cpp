#include "core/BarLayoutMetrics.hpp"
#include "core/BarPopupController.hpp"
#include "core/BarStatusOrderStore.hpp"
#include "core/BarController.hpp"
#include "services/BarClockService.hpp"
#include "statusnotifier/StatusNotifierItemModel.hpp"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIdentityProxyModel>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(Astrea_EffectsPlugin)

using namespace Astrea::StatusNotifier;

class ReorderTrayProxy final : public QIdentityProxyModel {
public:
    explicit ReorderTrayProxy(bool exposeIdRole) : m_exposeIdRole(exposeIdRole) {}
    QHash<int, QByteArray> roleNames() const override {
        auto roles = QIdentityProxyModel::roleNames();
        if (!m_exposeIdRole)
            roles.remove(StatusNotifierItemModel::IdRole);
        return roles;
    }
private:
    bool m_exposeIdRole;
};

class ReorderTrayService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *itemModel READ itemModel CONSTANT)
    Q_PROPERTY(int presentationRevision READ presentationRevision CONSTANT)

public:
    explicit ReorderTrayService(bool exposeIdRole = true) : proxy(exposeIdRole) {
        proxy.setSourceModel(&model);
    }
    QAbstractItemModel *itemModel() { return &proxy; }
    int presentationRevision() const { return 1; }
    void add(const QString &service = QStringLiteral("org.example.Chat")) {
        snapshot.address = {service, QStringLiteral("/StatusNotifierItem"), service};
        snapshot.id = QStringLiteral("chat");
        snapshot.title = QStringLiteral("Chat");
        snapshot.ready = true;
        model.upsert(snapshot, {});
    }
    void remove() { model.removeKey(snapshot.address.key()); }
    Q_INVOKABLE bool hasUsableMenuForItem(const QString &) const { return false; }
    Q_INVOKABLE QString tooltipTitleForItem(const QString &) const { return {}; }
    Q_INVOKABLE void activate(const QString &, int, int) { ++activations; }
    Q_INVOKABLE void secondaryActivate(const QString &, int, int) { ++secondaryActivations; }
    Q_INVOKABLE void contextMenu(const QString &, int, int) { ++contextMenus; }
    Q_INVOKABLE void scroll(const QString &, int, const QString &) { ++scrolls; }

    StatusNotifierItemModel model;
    ReorderTrayProxy proxy;
    ItemSnapshot snapshot;
    int activations = 0;
    int secondaryActivations = 0;
    int contextMenus = 0;
    int scrolls = 0;
};

class ReorderAudioService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool defaultStateAvailable READ available CONSTANT)
    Q_PROPERTY(double volume READ volume CONSTANT)
    Q_PROPERTY(bool muted READ muted CONSTANT)
public:
    bool available() const { return true; }
    double volume() const { return 50; }
    bool muted() const { return false; }
    Q_INVOKABLE void adjustVolume(double delta) { lastDelta = delta; }
    double lastDelta = 0;
};

class ReorderBarController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *statusOrderStore READ statusOrderStore CONSTANT)
public:
    explicit ReorderBarController(BarStatusOrderStore *store) : m_store(store) {}
    QObject *statusOrderStore() const { return m_store; }
private:
    BarStatusOrderStore *m_store;
};

static QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name)
        return root;
    for (QQuickItem *child : root->childItems()) {
        if (auto *found = findItem(child, name))
            return found;
    }
    return nullptr;
}

struct ReorderFixture {
    QTemporaryDir directory;
    QString configPath = directory.filePath(QStringLiteral("order.json"));
    BarStatusOrderStore store{configPath};
    ReorderBarController controller{&store};
    ReorderTrayService tray;
    ReorderAudioService audio;
    BarClockService clock;
    BarPopupController popup;
    BarLayoutMetrics geometry;
    QQmlEngine engine;
    QString error;
    std::unique_ptr<QQuickWindow> window;

    explicit ReorderFixture(bool exposeIdRole = true) : tray(exposeIdRole) {
        if (!directory.isValid()) {
            error = QStringLiteral("Could not create the isolated test configuration directory");
            return;
        }
        tray.add();
        clock.start();
        window = createWindow();
    }

    std::unique_ptr<QQuickWindow> createWindow(bool injectStore = true) {
        QQmlComponent component(&engine,
            QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Bar/qml/StatusSurface.qml")));
        auto *object = component.createWithInitialProperties({
            {QStringLiteral("barController"), injectStore
                ? QVariant::fromValue(&controller) : QVariant::fromValue<QObject *>(nullptr)},
            {QStringLiteral("statusNotifierService"), QVariant::fromValue(&tray)},
            {QStringLiteral("audioService"), QVariant::fromValue(&audio)},
            {QStringLiteral("clockService"), QVariant::fromValue(&clock)},
            {QStringLiteral("popupController"), QVariant::fromValue(&popup)},
            {QStringLiteral("barGeometry"), QVariant::fromValue(&geometry)},
            {QStringLiteral("outputWidth"), 1200},
        });
        for (const auto &message : component.errors())
            error += message.toString() + QLatin1Char('\n');
        auto result = std::unique_ptr<QQuickWindow>(qobject_cast<QQuickWindow *>(object));
        if (!result)
            delete object;
        else {
            result->show();
            QCoreApplication::processEvents();
            if (auto *layout = findItem(result->contentItem(), QStringLiteral("statusItemLayout")))
                layout->setProperty("holdInterval", 50);
        }
        return result;
    }

    QQuickItem *item(const QString &name) const { return findItem(window->contentItem(), name); }
    QPoint center(QQuickItem *item) const {
        return item->mapToItem(window->contentItem(), QPointF(item->width() / 2,
                                                              item->height() / 2)).toPoint();
    }
    void hold(QQuickItem *item) {
        QTest::mousePress(window.get(), Qt::LeftButton, Qt::NoModifier, center(item));
        QTest::qWait(100);
    }
    void drop(const QPoint &point) {
        QTest::mouseMove(window.get(), point, 20);
        QTest::mouseRelease(window.get(), Qt::LeftButton, Qt::NoModifier, point);
        QCoreApplication::processEvents();
    }
};

class TopbarReorderQmlTest final : public QObject {
    Q_OBJECT
private slots:
    void trayAndThemedIndicatorsUseOneImageComponent();
    void ordinaryButtonsAndWheelKeepTheirActions();
    void heldTrayItemMovesBetweenSystemIndicatorsWithoutActivation();
    void holdWithoutMovementAndRemovalDoNotCommit();
    void clockReorderPropagatesToAnotherOutput();
    void draggingClosesThePopupAndSubsequentClicksUseTheNewAnchor();
    void missingStoreStillAllowsSessionReordering();
    void smallPointerJitterDoesNotCancelTheHold();
    void trayItemCanMoveToBothEnds();
    void draggingAwayAndBackDoesNotChangeOrSaveTheOrder();
    void productionControllerExposesTheSharedStoreToQml();
    void trayWithoutIdentifierRoleStillInstantiatesAndActivates();
};

void TopbarReorderQmlTest::trayAndThemedIndicatorsUseOneImageComponent()
{
    ReorderFixture fixture;
    QVERIFY2(fixture.window, qPrintable(fixture.error));
    auto *trayIcon = fixture.item(QStringLiteral("trayItemIcon"));
    auto *network = fixture.item(QStringLiteral("networkIndicator"));
    QVERIFY(trayIcon);
    QVERIFY(network);
    auto *themedIcon = findItem(network, QStringLiteral("barThemedIconArtwork"));
    QVERIFY(themedIcon);
    QQmlComponent sharedComponent(&fixture.engine, QUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Shell/Bar/qml/components/BarIcon.qml")));
    std::unique_ptr<QObject> sharedIcon(sharedComponent.createWithInitialProperties({
        {QStringLiteral("iconSize"), 24},
    }));
    QVERIFY2(sharedIcon, qPrintable(sharedComponent.errorString()));
    QCOMPARE(sharedIcon->property("width").toInt(), 24);
    QCOMPARE(sharedIcon->property("sourceSize").toSize(), QSize(24, 24));
    QCOMPARE(trayIcon->property("iconSize").toInt(), 16);
    QCOMPARE(themedIcon->property("iconSize").toInt(), 16);
    QCOMPARE(trayIcon->property("sourceSize").toSize(), QSize(16, 16));
    QVERIFY(trayIcon->property("smooth").toBool());
    QVERIFY(themedIcon->property("smooth").toBool());
}

void TopbarReorderQmlTest::ordinaryButtonsAndWheelKeepTheirActions()
{
    ReorderFixture fixture;
    QVERIFY2(fixture.window, qPrintable(fixture.error));
    auto *tray = fixture.item(QStringLiteral("trayItem"));
    auto *network = fixture.item(QStringLiteral("networkIndicator"));
    auto *volume = fixture.item(QStringLiteral("volumeIndicator"));
    QVERIFY(tray && network && volume);
    QTest::mouseClick(fixture.window.get(), Qt::LeftButton, Qt::NoModifier, fixture.center(tray));
    QTest::mouseClick(fixture.window.get(), Qt::MiddleButton, Qt::NoModifier, fixture.center(tray));
    QTest::mouseClick(fixture.window.get(), Qt::RightButton, Qt::NoModifier, fixture.center(tray));
    QCOMPARE(fixture.tray.activations, 1);
    QCOMPARE(fixture.tray.secondaryActivations, 1);
    QCOMPARE(fixture.tray.contextMenus, 1);
    QTest::wheelEvent(fixture.window.get(), fixture.center(volume), QPoint(0, 120));
    QCOMPARE(fixture.audio.lastDelta, 2.0);
    QTest::mouseClick(fixture.window.get(), Qt::MiddleButton, Qt::NoModifier, fixture.center(network));
    QVERIFY(!fixture.popup.surfaceRequired());
    QTest::mouseClick(fixture.window.get(), Qt::LeftButton, Qt::NoModifier, fixture.center(network));
    QCOMPARE(fixture.popup.kind(), BarPopupController::PopupKind::Network);
    auto *pill = fixture.item(QStringLiteral("statusPill"));
    QVERIFY(pill);
    const qreal local = network->mapToItem(pill, QPointF(network->width() / 2, 0)).x();
    QCOMPARE(fixture.popup.anchorX(), fixture.window->property("statusLeft").toInt() + qRound(local));
    QVERIFY(!QFileInfo::exists(fixture.configPath));
}

void TopbarReorderQmlTest::heldTrayItemMovesBetweenSystemIndicatorsWithoutActivation()
{
    ReorderFixture fixture;
    QVERIFY2(fixture.window, qPrintable(fixture.error));
    auto *tray = fixture.item(QStringLiteral("trayItem"));
    auto *volume = fixture.item(QStringLiteral("volumeIndicator"));
    auto *clock = fixture.item(QStringLiteral("clock"));
    auto *layout = fixture.item(QStringLiteral("statusItemLayout"));
    QVERIFY(tray && volume && clock && layout);
    const QString stableKey = tray->property("orderKey").toString();
    QVERIFY(!stableKey.isEmpty());
    fixture.hold(tray);
    QVERIFY(layout->property("dragActive").toBool());
    QPoint target = clock->mapToItem(fixture.window->contentItem(), QPointF(0, clock->height() / 2)).toPoint();
    target.rx() -= 2;
    fixture.drop(target);
    QTRY_VERIFY(!layout->property("dragActive").toBool());
    QTRY_VERIFY(fixture.center(tray).x() > fixture.center(volume).x());
    QVERIFY(fixture.center(tray).x() < fixture.center(clock).x());
    QCOMPARE(fixture.tray.activations, 0);
    QVERIFY(QFileInfo::exists(fixture.configPath));
    BarStatusOrderStore restored(fixture.configPath);
    QCOMPARE(restored.order(), fixture.store.order());

    fixture.tray.remove();
    QCoreApplication::processEvents();
    fixture.tray.add(QStringLiteral("org.example.ChatReconnected"));
    QTRY_VERIFY(fixture.item(QStringLiteral("trayItem")) != nullptr);
    tray = fixture.item(QStringLiteral("trayItem"));
    QCOMPARE(tray->property("orderKey").toString(), stableKey);
    QTRY_VERIFY(fixture.center(tray).x() > fixture.center(volume).x());
}

void TopbarReorderQmlTest::holdWithoutMovementAndRemovalDoNotCommit()
{
    ReorderFixture fixture;
    QVERIFY2(fixture.window, qPrintable(fixture.error));
    auto *tray = fixture.item(QStringLiteral("trayItem"));
    auto *layout = fixture.item(QStringLiteral("statusItemLayout"));
    QVERIFY(tray && layout);
    const QPoint position = fixture.center(tray);
    fixture.hold(tray);
    QVERIFY(layout->property("dragActive").toBool());
    fixture.drop(position);
    QCOMPARE(fixture.tray.activations, 0);
    QVERIFY(!QFileInfo::exists(fixture.configPath));
    fixture.hold(tray);
    QVERIFY(layout->property("dragActive").toBool());
    fixture.tray.remove();
    QTRY_VERIFY(!layout->property("dragActive").toBool());
    QTest::mouseRelease(fixture.window.get(), Qt::LeftButton, Qt::NoModifier, position);
    QCOMPARE(fixture.tray.activations, 0);
    QVERIFY(!QFileInfo::exists(fixture.configPath));
}

void TopbarReorderQmlTest::clockReorderPropagatesToAnotherOutput()
{
    ReorderFixture fixture;
    QVERIFY2(fixture.window, qPrintable(fixture.error));
    auto other = fixture.createWindow();
    QVERIFY2(other, qPrintable(fixture.error));
    auto *clock = fixture.item(QStringLiteral("clock"));
    auto *tray = fixture.item(QStringLiteral("trayItem"));
    auto *button = fixture.item(QStringLiteral("clockButton"));
    auto *layout = fixture.item(QStringLiteral("statusItemLayout"));
    QVERIFY(clock && tray && button && layout);
    QVERIFY(!button->property("interactive").toBool());
    fixture.hold(clock);
    QVERIFY(layout->property("dragActive").toBool());
    fixture.drop(QPoint(10, fixture.center(clock).y()));
    QTRY_VERIFY(fixture.center(clock).x() < fixture.center(tray).x());
    auto *otherClock = findItem(other->contentItem(), QStringLiteral("clock"));
    auto *otherTray = findItem(other->contentItem(), QStringLiteral("trayItem"));
    QVERIFY(otherClock && otherTray);
    QTRY_VERIFY(otherClock->mapToItem(other->contentItem(), QPointF()).x()
                < otherTray->mapToItem(other->contentItem(), QPointF()).x());
    QVERIFY(!fixture.popup.surfaceRequired());
}

void TopbarReorderQmlTest::draggingClosesThePopupAndSubsequentClicksUseTheNewAnchor()
{
    ReorderFixture fixture;
    QVERIFY2(fixture.window, qPrintable(fixture.error));
    auto *network = fixture.item(QStringLiteral("networkIndicator"));
    auto *volume = fixture.item(QStringLiteral("volumeIndicator"));
    auto *clock = fixture.item(QStringLiteral("clock"));
    auto *layout = fixture.item(QStringLiteral("statusItemLayout"));
    QVERIFY(network && volume && clock && layout);
    QTest::mouseClick(fixture.window.get(), Qt::LeftButton, Qt::NoModifier, fixture.center(network));
    QVERIFY(fixture.popup.isOpen());
    const int originalAnchor = fixture.popup.anchorX();
    fixture.hold(network);
    QVERIFY(layout->property("dragActive").toBool());
    QVERIFY(!fixture.popup.isOpen());
    QPoint target = clock->mapToItem(fixture.window->contentItem(), QPointF(0, clock->height() / 2)).toPoint();
    target.rx() -= 2;
    fixture.drop(target);
    QTRY_VERIFY(fixture.center(network).x() > fixture.center(volume).x());
    fixture.popup.completeClose();
    QTest::mouseClick(fixture.window.get(), Qt::LeftButton, Qt::NoModifier, fixture.center(network));
    QVERIFY(fixture.popup.isOpen());
    auto *pill = fixture.item(QStringLiteral("statusPill"));
    QVERIFY(pill);
    const qreal local = network->mapToItem(pill, QPointF(network->width() / 2, 0)).x();
    QCOMPARE(fixture.popup.anchorX(), fixture.window->property("statusLeft").toInt() + qRound(local));
    QVERIFY(fixture.popup.anchorX() != originalAnchor);
}

void TopbarReorderQmlTest::missingStoreStillAllowsSessionReordering()
{
    ReorderFixture fixture;
    QVERIFY2(fixture.window, qPrintable(fixture.error));
    fixture.window = fixture.createWindow(false);
    QVERIFY2(fixture.window, qPrintable(fixture.error));
    auto *clock = fixture.item(QStringLiteral("clock"));
    auto *tray = fixture.item(QStringLiteral("trayItem"));
    auto *layout = fixture.item(QStringLiteral("statusItemLayout"));
    QVERIFY(clock && tray && layout);
    fixture.hold(clock);
    QVERIFY(layout->property("dragActive").toBool());
    fixture.drop(QPoint(10, fixture.center(clock).y()));
    QTRY_VERIFY(fixture.center(clock).x() < fixture.center(tray).x());
    QVERIFY(!QFileInfo::exists(fixture.configPath));
}

void TopbarReorderQmlTest::smallPointerJitterDoesNotCancelTheHold()
{
    ReorderFixture fixture;
    QVERIFY2(fixture.window, qPrintable(fixture.error));
    auto *tray = fixture.item(QStringLiteral("trayItem"));
    auto *layout = fixture.item(QStringLiteral("statusItemLayout"));
    QVERIFY(tray && layout);
    const QPoint position = fixture.center(tray);
    QTest::mousePress(fixture.window.get(), Qt::LeftButton, Qt::NoModifier, position);
    QTest::mouseMove(fixture.window.get(), position + QPoint(2, 0), 10);
    QTest::qWait(100);
    QVERIFY(layout->property("dragActive").toBool());
    fixture.drop(position + QPoint(2, 0));
    QCOMPARE(fixture.tray.activations, 0);
    QVERIFY(!QFileInfo::exists(fixture.configPath));
}

void TopbarReorderQmlTest::trayItemCanMoveToBothEnds()
{
    ReorderFixture fixture;
    QVERIFY2(fixture.window, qPrintable(fixture.error));
    auto *tray = fixture.item(QStringLiteral("trayItem"));
    auto *clock = fixture.item(QStringLiteral("clock"));
    auto *network = fixture.item(QStringLiteral("networkIndicator"));
    QVERIFY(tray && clock && network);
    fixture.hold(tray);
    fixture.drop(QPoint(fixture.window->width() - 10, fixture.center(tray).y()));
    QTRY_VERIFY(fixture.center(tray).x() > fixture.center(clock).x());
    fixture.hold(tray);
    fixture.drop(QPoint(10, fixture.center(tray).y()));
    QTRY_VERIFY(fixture.center(tray).x() < fixture.center(network).x());
    QCOMPARE(fixture.tray.activations, 0);
}

void TopbarReorderQmlTest::draggingAwayAndBackDoesNotChangeOrSaveTheOrder()
{
    ReorderFixture fixture;
    QVERIFY2(fixture.window, qPrintable(fixture.error));
    auto *tray = fixture.item(QStringLiteral("trayItem"));
    auto *clock = fixture.item(QStringLiteral("clock"));
    auto *layout = fixture.item(QStringLiteral("statusItemLayout"));
    QVERIFY(tray && clock && layout);
    const QStringList original = fixture.store.order();
    const QPoint origin = fixture.center(tray);
    fixture.hold(tray);
    QVERIFY(layout->property("dragActive").toBool());
    QPoint target = clock->mapToItem(fixture.window->contentItem(), QPointF(0, clock->height() / 2)).toPoint();
    target.rx() -= 2;
    QTest::mouseMove(fixture.window.get(), target, 20);
    QTest::mouseMove(fixture.window.get(), origin, 20);
    fixture.drop(origin);
    QCOMPARE(fixture.store.order(), original);
    QCOMPARE(fixture.tray.activations, 0);
    QVERIFY(!QFileInfo::exists(fixture.configPath));
}

void TopbarReorderQmlTest::productionControllerExposesTheSharedStoreToQml()
{
    BarController controller(nullptr, nullptr);
    ReorderFixture fixture;
    QVERIFY2(fixture.window, qPrintable(fixture.error));
    fixture.window->setProperty("barController", QVariant::fromValue(&controller));
    QCoreApplication::processEvents();
    auto *layout = fixture.item(QStringLiteral("statusItemLayout"));
    QVERIFY(layout);
    QCOMPARE(layout->property("orderStore").value<QObject *>(), controller.statusOrderStore());
}

void TopbarReorderQmlTest::trayWithoutIdentifierRoleStillInstantiatesAndActivates()
{
    ReorderFixture fixture(false);
    QVERIFY2(fixture.window, qPrintable(fixture.error));
    auto *tray = fixture.item(QStringLiteral("trayItem"));
    QVERIFY(tray);
    QVERIFY(tray->property("orderKey").toString().startsWith(QStringLiteral("tray-fallback:")));
    QTest::mouseClick(fixture.window.get(), Qt::LeftButton, Qt::NoModifier, fixture.center(tray));
    QCOMPARE(fixture.tray.activations, 1);
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication application(argc, argv);
    TopbarReorderQmlTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TopbarReorderQmlTest.moc"
