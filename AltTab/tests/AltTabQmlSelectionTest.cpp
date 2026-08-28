#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlExtensionPlugin>
#include <QQmlEngine>
#include <QQuickItem>
#include <QTest>

#include <memory>

#include "core/AltTabWindowModel.hpp"
#include "icons/AstreaIconProvider.hpp"

Q_IMPORT_QML_PLUGIN(Astrea_SharedPlugin)

class FakeAltTabQmlController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ open WRITE setOpen NOTIFY openChanged)

public:
    bool open() const { return m_open; }
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
    Q_INVOKABLE void cancel() {}
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

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    AltTabQmlSelectionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "AltTabQmlSelectionTest.moc"
