#include <QGuiApplication>
#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSaveFile>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QUrlQuery>

#include <memory>

Q_IMPORT_QML_PLUGIN(Astrea_SharedPlugin)
Q_IMPORT_QML_PLUGIN(Astrea_EffectsPlugin)

class FakeSpotlightController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool surfaceVisible READ surfaceVisible CONSTANT)
    Q_PROPERTY(bool open READ open CONSTANT)
    Q_PROPERTY(QString query READ query NOTIFY queryChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QObject *resultsModel READ resultsModel CONSTANT)
    Q_PROPERTY(bool weatherEnabled READ weatherEnabled CONSTANT)
    Q_PROPERTY(bool weatherReady READ weatherReady CONSTANT)
    Q_PROPERTY(QString weatherTemp READ weatherTemp CONSTANT)
    Q_PROPERTY(QUrl weatherIconSource READ weatherIconSource CONSTANT)
public:
    bool surfaceVisible() const { return true; }
    bool open() const { return true; }
    QString query() const { return {}; }
    QString fontFamily() const { return QStringLiteral("Inter"); }
    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int index) { m_selectedIndex = index; emit selectedIndexChanged(); }
    QObject *resultsModel() const { return results; }
    QObject *results = nullptr;
    bool weatherEnabled() const { return false; }
    bool weatherReady() const { return false; }
    QString weatherTemp() const { return {}; }
    QUrl weatherIconSource() const { return {}; }
    int closeCount = 0;
    Q_INVOKABLE void close() { ++closeCount; }
    Q_INVOKABLE void scheduleSearch(const QString &) {}
    Q_INVOKABLE void moveSelection(int) {}
    Q_INVOKABLE void activateCurrent() {}
    Q_INVOKABLE void launch(int) {}
    Q_INVOKABLE void beginAppDrag(int row) { dragRows.append(row); }
    Q_INVOKABLE void moveAppDrag(qreal x, qreal y) { lastDragPoint = QPointF(x, y); }
    Q_INVOKABLE void finishAppDrag() { ++dragFinishes; }
    Q_INVOKABLE void cancelAppDrag() { ++dragCancels; }
    QList<int> dragRows;
    QPointF lastDragPoint;
    int dragFinishes = 0;
    int dragCancels = 0;
signals:
    void focusRequested();
    void launchFailed(const QString &);
    void queryChanged();
    void selectedIndexChanged();
private:
    int m_selectedIndex = 0;
};

class FakeThemeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int shellStyle READ shellStyle CONSTANT)
    Q_PROPERTY(int navigationIconOverridesRevision READ navigationIconOverridesRevision CONSTANT)
    Q_PROPERTY(QString navigationIconTheme READ navigationIconTheme CONSTANT)
    Q_PROPERTY(QString navigationIconWeight READ navigationIconWeight CONSTANT)
    Q_PROPERTY(int themeComponentsRevision READ themeComponentsRevision NOTIFY revisionChanged)
public:
    int shellStyle() const { return 0; }
    int navigationIconOverridesRevision() const { return 0; }
    QString navigationIconTheme() const { return {}; }
    QString navigationIconWeight() const { return {}; }
    Q_INVOKABLE QUrl themedNavigationIcon(const QString &) const { return {}; }
    int themeComponentsRevision() const { return m_revision; }
    Q_INVOKABLE QUrl themedComponent(const QString &name) const
    {
        if (name != QStringLiteral("spotlightPanel") || m_componentUrl.isEmpty())
            return {};
        QUrl result = m_componentUrl;
        QUrlQuery query(result);
        query.addQueryItem(QStringLiteral("astreaRevision"), QString::number(m_revision));
        result.setQuery(query);
        return result;
    }
    void useExamplePanel(const QString &path)
    {
        m_componentUrl = QUrl::fromLocalFile(path);
        ++m_revision;
        emit revisionChanged();
    }
    void refreshComponents() { ++m_revision; emit revisionChanged(); }
signals:
    void revisionChanged();
private:
    QUrl m_componentUrl;
    int m_revision = 0;
};

class FakeI18n final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap messages MEMBER messages CONSTANT)
public:
    QVariantMap messages;
    Q_INVOKABLE QString tr(const QString &, const QString &fallback) const { return fallback; }
};

class SpotlightQmlPanelTest final : public QObject {
    Q_OBJECT
private slots:
    void mainWindowKeepsPanelPlacementFocusBackdropAndOutsideHitTesting();
    void resultRowDragsTheAppAndStillLaunchesOnClick();
};

void SpotlightQmlPanelTest::mainWindowKeepsPanelPlacementFocusBackdropAndOutsideHitTesting()
{
    FakeSpotlightController controller;
    FakeThemeController themeController;
    FakeI18n i18n;
    QTemporaryDir fixture(QStringLiteral("/mnt/Aether/Desktop/GitHub/eclipse-spotlight-panel-XXXXXX"));
    QVERIFY(fixture.isValid());
    QFile example(QStringLiteral(ASTREA_SPOTLIGHT_EXAMPLE_PANEL_QML));
    QVERIFY(example.open(QIODevice::ReadOnly));
    const QString fixturePanelPath = fixture.filePath(QStringLiteral("SpotlightPanel.qml"));
    QSaveFile fixturePanel(fixturePanelPath);
    QVERIFY(fixturePanel.open(QIODevice::WriteOnly));
    const QByteArray exampleQml = example.readAll();
    QCOMPARE(fixturePanel.write(exampleQml), qint64(exampleQml.size()));
    QVERIFY(fixturePanel.commit());

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(ASTREA_QML_IMPORT_PATH));
    engine.rootContext()->setContextProperty(QStringLiteral("SpotlightController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.rootContext()->setContextProperty(QStringLiteral("AstreaI18n"), &i18n);

    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(ASTREA_SPOTLIGHT_MAIN_QML)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(object.get());
    QVERIFY(window);
    object->setProperty("surfaceReady", true);
    window->show();
    QTRY_VERIFY(window->isVisible());
    QCoreApplication::processEvents();

    auto *loader = qobject_cast<QQuickItem *>(window->findChild<QObject *>(QStringLiteral("spotlightPanelLoader")));
    QVERIFY(loader);
    auto *loaded = qobject_cast<QQuickItem *>(loader->property("item").value<QObject *>());
    QVERIFY(loaded);
    QVERIFY(qAbs(loader->width() - loaded->width()) < 1.0);
    QVERIFY(qAbs(loader->height() - loaded->height()) < 1.0);
    QVERIFY(qAbs(loader->x() + loader->width() / 2.0 - window->width() / 2.0) < 1.0);
    QVERIFY(qAbs(loader->y() - window->height() * 0.25) < 1.0);
    QVERIFY(loaded->property("radius").toDouble() >= 0.0);
    auto *search = qobject_cast<QQuickItem *>(window->findChild<QObject *>(QStringLiteral("spotlightSearchInput")));
    QVERIFY(search);
    QVERIFY(QMetaObject::invokeMethod(loaded, "focusSearch"));
    QTRY_VERIFY(search->hasFocus());

    themeController.useExamplePanel(fixturePanelPath);
    QTRY_VERIFY_WITH_TIMEOUT(!loader->property("usingFallback").toBool(), 3000);
    QVERIFY2(!loader->property("usingFallback").toBool(),
             qPrintable(loader->property("error").toString()));
    loaded = qobject_cast<QQuickItem *>(loader->property("item").value<QObject *>());
    QVERIFY(loaded);
    QCOMPARE(loaded->objectName(), QStringLiteral("themeExampleSpotlightPanel"));
    QVERIFY(qAbs(loader->width() - loaded->width()) < 1.0);
    QVERIFY(qAbs(loader->height() - loaded->height()) < 1.0);
    QVERIFY(QMetaObject::invokeMethod(loaded, "focusSearch"));
    auto *externalSearch = qobject_cast<QQuickItem *>(
        loaded->findChild<QObject *>(QStringLiteral("themeExampleSpotlightSearch")));
    QVERIFY(externalSearch);
    QTRY_VERIFY(externalSearch->hasFocus());

    QByteArray editedQml = exampleQml;
    QVERIFY(editedQml.contains("themeExampleSpotlightPanel"));
    editedQml.replace("themeExampleSpotlightPanel", "editedSpotlightPanel");
    QSaveFile editedPanel(fixturePanelPath);
    QVERIFY(editedPanel.open(QIODevice::WriteOnly));
    QCOMPARE(editedPanel.write(editedQml), qint64(editedQml.size()));
    QVERIFY(editedPanel.commit());
    themeController.refreshComponents();
    QTRY_COMPARE_WITH_TIMEOUT(loader->property("item").value<QObject *>()->objectName(),
                              QStringLiteral("editedSpotlightPanel"), 3000);
    loaded = qobject_cast<QQuickItem *>(loader->property("item").value<QObject *>());
    QVERIFY(loaded);
    QVERIFY(qAbs(loader->width() - loaded->width()) < 1.0);
    QVERIFY(qAbs(loader->height() - loaded->height()) < 1.0);
    QVERIFY(QMetaObject::invokeMethod(loaded, "focusSearch"));

    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(2, 2));
    QCoreApplication::processEvents();
    QCOMPARE(controller.closeCount, 1);
    const QPoint inside(qRound(loader->x() + loader->width() / 2.0),
                        qRound(loader->y() + loader->height() / 2.0));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, inside);
    QCoreApplication::processEvents();
    QCOMPARE(controller.closeCount, 1);
}

void SpotlightQmlPanelTest::resultRowDragsTheAppAndStillLaunchesOnClick()
{
    QStandardItemModel results;
    enum { Name = Qt::UserRole + 1, Icon, Entry, WmClass };
    results.setItemRoleNames({{Name, "name"}, {Icon, "iconName"}, {Entry, "entryId"},
                              {WmClass, "startupWmClass"}});
    auto *row = new QStandardItem;
    row->setData(QStringLiteral("One"), Name);
    row->setData(QStringLiteral("one"), Icon);
    row->setData(QStringLiteral("one"), Entry);
    row->setData(QStringLiteral("one"), WmClass);
    results.appendRow(row);

    FakeSpotlightController controller;
    controller.results = &results;
    FakeThemeController themeController;
    FakeI18n i18n;
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(ASTREA_QML_IMPORT_PATH));
    engine.rootContext()->setContextProperty(QStringLiteral("SpotlightController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.rootContext()->setContextProperty(QStringLiteral("AstreaI18n"), &i18n);

    const QUrl listUrl = QUrl::fromLocalFile(QStringLiteral(ASTREA_SPOTLIGHT_MAIN_QML))
        .resolved(QUrl(QStringLiteral("components/SpotlightResultsList.qml")));
    QQmlComponent component(&engine, listUrl);
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(800, 500);
    std::unique_ptr<QObject> object(component.createWithInitialProperties(
        {{QStringLiteral("queryText"), QStringLiteral("one")}}));
    QVERIFY2(object, qPrintable(component.errorString()));
    auto *list = qobject_cast<QQuickItem *>(object.get());
    QVERIFY(list);
    list->setParentItem(window.contentItem());
    list->setWidth(600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_COMPARE(list->property("count").toInt(), 1);

    QQuickItem *ghost = nullptr;
    for (QQuickItem *child : window.contentItem()->childItems()) {
        if (child->objectName() == QStringLiteral("spotlightAppDragGhost"))
            ghost = child;
    }
    QVERIFY(ghost);
    QCOMPARE(ghost->parentItem(), window.contentItem());
    QVERIFY(!ghost->isVisible());

    QTRY_VERIFY(list->implicitHeight() > 20);
    const QPoint start(100, qRound(list->implicitHeight()) - 20);
    list->setHeight(list->implicitHeight());
    QSignalSpy launched(list, SIGNAL(launchRequested(int)));
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTRY_COMPARE(launched.size(), 1);
    QVERIFY(controller.dragRows.isEmpty());

    QTest::mouseMove(&window, start);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    for (int step = 1; step <= 6; ++step)
        QTest::mouseMove(&window, start + QPoint(step * 20, step * 30));
    QTRY_COMPARE(controller.dragRows, QList<int>{0});
    QTRY_VERIFY(ghost->isVisible());
    const QPoint end = start + QPoint(120, 180);
    QTRY_COMPARE(controller.lastDragPoint, QPointF(end));
    QCOMPARE(ghost->x() + ghost->width() / 2, qreal(end.x()));
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, end);
    QTRY_COMPARE(controller.dragFinishes, 1);
    QTRY_VERIFY(!ghost->isVisible());
    // The drag is not also a click.
    QCOMPARE(launched.size(), 1);
    QCOMPARE(controller.dragCancels, 0);
}

QTEST_MAIN(SpotlightQmlPanelTest)
#include "SpotlightQmlPanelTest.moc"
