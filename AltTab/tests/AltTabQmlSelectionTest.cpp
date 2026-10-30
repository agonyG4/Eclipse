#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlExtensionPlugin>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

#include <memory>

#include "core/AltTabWindowModel.hpp"
#include "icons/AstreaIconProvider.hpp"

Q_IMPORT_QML_PLUGIN(Astrea_SharedPlugin)
Q_IMPORT_QML_PLUGIN(Astrea_EffectsPlugin)

class FakeAltTabQmlController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ open WRITE setOpen NOTIFY openChanged)
    Q_PROPERTY(bool surfaceVisible READ surfaceVisible CONSTANT)

public:
    bool open() const { return m_open; }
    bool surfaceVisible() const { return true; }
    int cancelCount = 0;
    void setOpen(bool open)
    {
        if (m_open == open)
            return;
        m_open = open;
        emit openChanged();
    }

    Q_INVOKABLE void preview(int index) { Q_UNUSED(index); }
    Q_INVOKABLE void commitIndex(int index) { Q_UNUSED(index); }
    Q_INVOKABLE void commit() {}
    Q_INVOKABLE void cancel() { ++cancelCount; }
    Q_INVOKABLE void step(int direction) { Q_UNUSED(direction); }

signals:
    void openChanged();

private:
    bool m_open = true;
};

class AltTabQmlSelectionTest final : public QObject {
    Q_OBJECT

private slots:
    void realPanelKeepsSelectionIndependentFromActiveAndHover();
    void selectionChangesPresentationWithoutChangingIconQuality();
    void mainWindowKeepsLoadedPanelGeometryFocusAndOutsideHitTesting();
};

class FakeThemeQmlController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int shellStyle READ shellStyle CONSTANT)
    Q_PROPERTY(int themeComponentsRevision READ themeComponentsRevision NOTIFY revisionChanged)
public:
    int shellStyle() const { return 0; }
    int themeComponentsRevision() const { return m_revision; }
    Q_INVOKABLE QUrl themedComponent(const QString &name) const
    {
        return name == QStringLiteral("altTabPanel") ? m_componentUrl : QUrl{};
    }
    void useExamplePanel()
    {
        m_componentUrl = QUrl::fromLocalFile(QStringLiteral(ASTREA_ALTTAB_EXAMPLE_PANEL_QML));
        ++m_revision;
        emit revisionChanged();
    }
signals:
    void revisionChanged();
private:
    QUrl m_componentUrl;
    int m_revision = 0;
};

static void collectDelegates(QQuickItem *item, QVector<QQuickItem *> &delegates)
{
    if (item->objectName() == QStringLiteral("altTabWindowDelegate"))
        delegates.append(item);
    for (QQuickItem *child : item->childItems())
        collectDelegates(child, delegates);
}

static QVector<QQuickItem *> delegatesFor(QQuickItem *root)
{
    QVector<QQuickItem *> delegates;
    collectDelegates(root, delegates);
    return delegates;
}

static QQuickItem *iconFor(QQuickItem *delegate)
{
    if (!delegate)
        return nullptr;
    for (QQuickItem *child : delegate->childItems()) {
        if (child->property("effectiveSourcePixelSize").isValid())
            return child;
        if (QQuickItem *nested = iconFor(child))
            return nested;
    }
    return nullptr;
}

void AltTabQmlSelectionTest::realPanelKeepsSelectionIndependentFromActiveAndHover()
{
    AltTabWindowModel model;
    QVector<WindowInfo> windows;
    for (const auto &entry : QVector<QPair<QString, bool>>{
             {QStringLiteral("A"), true}, {QStringLiteral("B"), false},
             {QStringLiteral("C"), false}}) {
        WindowInfo window;
        window.windowId = WindowId{entry.first};
        window.displayName = entry.first;
        window.appId = entry.first;
        window.title = entry.first;
        window.isActive = entry.second;
        windows.append(window);
    }
    model.setWindows(windows);
    model.setSelectedIndex(1);

    FakeAltTabQmlController controller;
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(ASTREA_QML_IMPORT_PATH));
    engine.addImageProvider(QStringLiteral("astrea-icon"), new AstreaIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("AltTabController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("AltTabWindowModel"), &model);

    QQmlComponent component(&engine,
                            QUrl::fromLocalFile(QStringLiteral(ASTREA_ALTTAB_PANEL_QML)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.create());
    QVERIFY2(panel != nullptr, qPrintable(component.errorString()));

    auto *panelItem = qobject_cast<QQuickItem *>(panel.get());
    QVERIFY(panelItem);
    QCoreApplication::processEvents();

    const auto assertSelection = [&](int expected) {
        const QVector<QQuickItem *> delegates = delegatesFor(panelItem);
        QVERIFY2(delegates.size() == 3, qPrintable(
            QStringLiteral("expected three delegates, got %1").arg(delegates.size())));
        int selectedCount = 0;
        for (QQuickItem *delegate : delegates) {
            const int row = delegate->property("windowIndex").toInt();
            const bool selected = delegate->property("windowSelected").toBool();
            if (selected)
                ++selectedCount;
            QCOMPARE(selected, row == expected);
        }
        QCOMPARE(selectedCount, 1);
    };

    assertSelection(1);
    const auto delegates = delegatesFor(panelItem);
    QQuickItem *first = nullptr;
    QQuickItem *second = nullptr;
    QQuickItem *third = nullptr;
    for (QQuickItem *delegate : delegates) {
        switch (delegate->property("windowIndex").toInt()) {
        case 0: first = delegate; break;
        case 1: second = delegate; break;
        case 2: third = delegate; break;
        default: break;
        }
    }
    QVERIFY(first);
    QVERIFY(second);
    QVERIFY(third);
    QCOMPARE(first->property("windowActive").toBool(), true);
    QCOMPARE(second->property("windowActive").toBool(), false);
    third->setProperty("windowHovered", true);
    QCOMPARE(third->property("windowHovered").toBool(), true);
    QCOMPARE(third->property("windowSelected").toBool(), false);

    for (int selected = 0; selected < 3; ++selected) {
        model.setSelectedIndex(selected);
        QCoreApplication::processEvents();
        assertSelection(selected);
    }
}

void AltTabQmlSelectionTest::selectionChangesPresentationWithoutChangingIconQuality()
{
    AltTabWindowModel model;
    QVector<WindowInfo> windows;
    for (const QString &name : {QStringLiteral("A"), QStringLiteral("B")}) {
        WindowInfo window;
        window.windowId = WindowId{name};
        window.displayName = name;
        window.appId = name;
        window.title = name;
        windows.append(window);
    }
    model.setWindows(windows);
    model.setSelectedIndex(0);

    FakeAltTabQmlController controller;
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(ASTREA_QML_IMPORT_PATH));
    engine.addImageProvider(QStringLiteral("astrea-icon"), new AstreaIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("AltTabController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("AltTabWindowModel"), &model);
    QQmlComponent component(&engine,
                            QUrl::fromLocalFile(QStringLiteral(ASTREA_ALTTAB_PANEL_QML)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.create());
    QVERIFY(panel);
    auto *panelItem = qobject_cast<QQuickItem *>(panel.get());
    QVERIFY(panelItem);
    QCoreApplication::processEvents();

    const QVector<QQuickItem *> delegates = delegatesFor(panelItem);
    QCOMPARE(delegates.size(), 2);
    QQuickItem *selectedDelegate = nullptr;
    QQuickItem *unselectedDelegate = nullptr;
    for (QQuickItem *delegate : delegates) {
        if (delegate->property("windowSelected").toBool())
            selectedDelegate = delegate;
        else
            unselectedDelegate = delegate;
    }
    QVERIFY(selectedDelegate && unselectedDelegate);
    QQuickItem *selectedIcon = iconFor(selectedDelegate);
    QQuickItem *unselectedIcon = iconFor(unselectedDelegate);
    QVERIFY(selectedIcon && unselectedIcon);
    QCOMPARE(selectedIcon->property("effectiveMaximumLogicalSize").toInt(), 84);
    QCOMPARE(unselectedIcon->property("effectiveMaximumLogicalSize").toInt(), 84);
    const int sourcePixels = selectedIcon->property("effectiveSourcePixelSize").toInt();
    const QString source = selectedIcon->property("resolvedSource").toString();
    QCOMPARE(sourcePixels, unselectedIcon->property("effectiveSourcePixelSize").toInt());

    model.setSelectedIndex(1);
    QTest::qWait(180);
    QCOMPARE(selectedIcon->property("resolvedSource").toString(), source);
    QCOMPARE(selectedIcon->property("effectiveSourcePixelSize").toInt(), sourcePixels);
    QCOMPARE(selectedIcon->property("effectiveMaximumLogicalSize").toInt(), 84);
    QVERIFY(selectedDelegate->property("windowSelected").toBool() == false);
    QVERIFY(unselectedDelegate->property("windowSelected").toBool() == true);
    QVERIFY(unselectedIcon->width() > selectedIcon->width());
}

void AltTabQmlSelectionTest::mainWindowKeepsLoadedPanelGeometryFocusAndOutsideHitTesting()
{
    AltTabWindowModel model;
    QVector<WindowInfo> windows;
    for (const QString &name : {QStringLiteral("A"), QStringLiteral("B")}) {
        WindowInfo info;
        info.windowId = WindowId{name};
        info.displayName = name;
        info.appId = name;
        info.title = name;
        windows.append(info);
    }
    model.setWindows(windows);
    FakeAltTabQmlController controller;
    FakeThemeQmlController themeController;
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(ASTREA_QML_IMPORT_PATH));
    engine.addImageProvider(QStringLiteral("astrea-icon"), new AstreaIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("AltTabController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("AltTabWindowModel"), &model);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);

    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(ASTREA_ALTTAB_MAIN_QML)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(object.get());
    QVERIFY(window);
    object->setProperty("surfaceReady", true);
    window->show();
    QTRY_VERIFY(window->isVisible());
    QCoreApplication::processEvents();

    QObject *loaderObject = window->findChild<QObject *>(QStringLiteral("altTabPanelLoader"));
    auto *loader = qobject_cast<QQuickItem *>(loaderObject);
    QVERIFY(loader);
    auto *loaded = qobject_cast<QQuickItem *>(loader->property("item").value<QObject *>());
    QVERIFY(loaded);
    QVERIFY(qAbs(loader->width() - loaded->width()) < 1.0);
    QVERIFY(qAbs(loader->height() - loaded->height()) < 1.0);
    QVERIFY(qAbs(loader->x() + loader->width() / 2.0 - window->width() / 2.0) < 1.0);
    QObject *backdrop = loaded->property("backdropItem").value<QObject *>();
    QVERIFY(qobject_cast<QQuickItem *>(backdrop));
    loaded->forceActiveFocus();
    QTRY_VERIFY(loaded->hasFocus());

    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(2, 2));
    QCoreApplication::processEvents();
    QCOMPARE(controller.cancelCount, 1);
    const QPoint inside(qRound(loader->x() + loader->width() / 2.0),
                        qRound(loader->y() + loader->height() / 2.0));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, inside);
    QCoreApplication::processEvents();
    QCOMPARE(controller.cancelCount, 1);

    themeController.useExamplePanel();
    QTRY_VERIFY_WITH_TIMEOUT(!loader->property("usingFallback").toBool(), 3000);
    loaded = qobject_cast<QQuickItem *>(loader->property("item").value<QObject *>());
    QVERIFY(loaded);
    QCOMPARE(loaded->objectName(), QStringLiteral("themeExampleAltTabPanel"));
    QVERIFY(qAbs(loader->width() - loaded->width()) < 1.0);
    QVERIFY(qAbs(loader->height() - loaded->height()) < 1.0);
    QVERIFY(qobject_cast<QQuickItem *>(loaded->property("backdropItem").value<QObject *>()));
    loaded->forceActiveFocus();
    QTRY_VERIFY(loaded->hasFocus());
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    AltTabQmlSelectionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "AltTabQmlSelectionTest.moc"
