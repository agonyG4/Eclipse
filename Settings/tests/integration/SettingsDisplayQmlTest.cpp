#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QUrl>
#include <Qt>
#include <QtTest>

namespace {

class FakeDisplayController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool available READ available CONSTANT)
  Q_PROPERTY(bool busy READ busy CONSTANT)
  Q_PROPERTY(QVariantList outputs READ outputs NOTIFY snapshotChanged)
  Q_PROPERTY(quint32 outputCount READ outputCount NOTIFY snapshotChanged)
  Q_PROPERTY(bool truncated READ truncated CONSTANT)
  Q_PROPERTY(QString lastError READ lastError CONSTANT)

public:
  bool available() const { return true; }
  bool busy() const { return false; }
  QVariantList outputs() const { return m_outputs; }
  quint32 outputCount() const { return quint32(m_outputs.size()); }
  bool truncated() const { return false; }
  QString lastError() const { return {}; }
  Q_INVOKABLE void refresh() { ++refreshCount; }

  void setOutput(const QVariantMap &output) {
    m_outputs = {output};
    emit snapshotChanged();
  }

  int refreshCount = 0;

signals:
  void snapshotChanged();

private:
  QVariantList m_outputs;
};

class FakeWallpaper final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QUrl effectivePreviewUrl READ effectivePreviewUrl CONSTANT)
  Q_PROPERTY(QString effectiveFit READ effectiveFit CONSTANT)

public:
  QUrl effectivePreviewUrl() const {
    return QUrl(QStringLiteral(
        "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' "
        "width='2' height='2'%3E%3Crect width='2' height='2' "
        "fill='%234d72c9'/%3E%3C/svg%3E"));
  }
  QString effectiveFit() const { return QStringLiteral("cover"); }
};

class FakeSettingsController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QObject *display READ display CONSTANT)
  Q_PROPERTY(QObject *wallpaper READ wallpaper CONSTANT)

public:
  FakeDisplayController *display() { return &m_display; }
  FakeWallpaper *wallpaper() { return &m_wallpaper; }

private:
  FakeDisplayController m_display;
  FakeWallpaper m_wallpaper;
};

QVariantMap
nativeOutput(const QString &name = QStringLiteral("DP-1"),
             const QString &model = QStringLiteral("Studio Panel")) {
  const QVariantList modes{
      QVariantMap{{QStringLiteral("width"), 1920},
                  {QStringLiteral("height"), 1080},
                  {QStringLiteral("refreshMilliHz"), 60000},
                  {QStringLiteral("preferred"), true},
                  {QStringLiteral("interlaced"), false}},
      QVariantMap{{QStringLiteral("width"), 1920},
                  {QStringLiteral("height"), 1080},
                  {QStringLiteral("refreshMilliHz"), 165000},
                  {QStringLiteral("preferred"), false},
                  {QStringLiteral("interlaced"), false}},
      QVariantMap{{QStringLiteral("width"), 1600},
                  {QStringLiteral("height"), 900},
                  {QStringLiteral("refreshMilliHz"), 59940},
                  {QStringLiteral("preferred"), false},
                  {QStringLiteral("interlaced"), false}},
  };
  return {{QStringLiteral("id"), QStringLiteral("output-1")},
          {QStringLiteral("name"), name},
          {QStringLiteral("make"), QStringLiteral("Astrea Labs")},
          {QStringLiteral("model"), model},
          {QStringLiteral("serial"), QVariant{}},
          {QStringLiteral("enabled"), true},
          {QStringLiteral("hasCurrentMode"), true},
          {QStringLiteral("currentWidth"), 1920},
          {QStringLiteral("currentHeight"), 1080},
          {QStringLiteral("currentRefreshMilliHz"), 165000},
          {QStringLiteral("hasPhysicalSize"), true},
          {QStringLiteral("physicalWidthMm"), 600},
          {QStringLiteral("physicalHeightMm"), 340},
          {QStringLiteral("scaleMilli"), 1000},
          {QStringLiteral("transform"), QStringLiteral("normal")},
          {QStringLiteral("x"), 0},
          {QStringLiteral("y"), 0},
          {QStringLiteral("focused"), true},
          {QStringLiteral("vrrState"), QStringLiteral("available")},
          {QStringLiteral("backend"), QStringLiteral("atomic")},
          {QStringLiteral("modes"), modes},
          {QStringLiteral("modesTruncated"), false}};
}

QObject *findObject(QQuickItem *item, const QString &name) {
  if (!item)
    return nullptr;
  if (item->objectName() == name)
    return item;
  for (QQuickItem *child : item->childItems()) {
    if (QObject *match = findObject(child, name))
      return match;
  }
  return nullptr;
}

class DisplayPageFixture final {
public:
  DisplayPageFixture() {
    m_settings.display()->setOutput(nativeOutput());
    m_engine.addImportPath(QStringLiteral("qrc:/"));
    m_engine.rootContext()->setContextProperty(
        QStringLiteral("SettingsController"), &m_settings);
    m_component = std::make_unique<QQmlComponent>(
        &m_engine,
        QUrl(QStringLiteral(
            "qrc:/qt/qml/Astrea/Settings/qml/pages/system/Display.qml")));
    m_window.resize(1000, 760);
  }

  QQuickItem *createPage() {
    QObject *created = m_component->create();
    if (!created)
      return nullptr;
    m_page = qobject_cast<QQuickItem *>(created);
    if (!m_page) {
      delete created;
      return nullptr;
    }
    m_page->setParentItem(m_window.contentItem());
    m_page->setSize(m_window.size());
    m_window.show();
    QCoreApplication::processEvents();
    return m_page;
  }

  ~DisplayPageFixture() {
    if (m_page) {
      m_page->setParentItem(nullptr);
      delete m_page;
    }
  }

  QString errors() const { return m_component->errorString(); }
  QObject *object(const QString &name) const {
    return findObject(m_page, name);
  }
  FakeDisplayController *display() { return m_settings.display(); }

private:
  FakeSettingsController m_settings;
  QQmlApplicationEngine m_engine;
  std::unique_ptr<QQmlComponent> m_component;
  QQuickWindow m_window;
  QQuickItem *m_page = nullptr;
};

} // namespace

class SettingsDisplayQmlTest final : public QObject {
  Q_OBJECT

private slots:
  void projectsAuthoritativeReadOnlyOutputSnapshot();
  void hostileOutputIdentityRemainsPlainText();
  void disabledOutputWithoutCurrentModeUsesSafePlaceholder();
};

void SettingsDisplayQmlTest::projectsAuthoritativeReadOnlyOutputSnapshot() {
  DisplayPageFixture fixture;
  QQuickItem *page = fixture.createPage();
  QVERIFY2(page != nullptr, qPrintable(fixture.errors()));
  QCOMPARE(page->objectName(), QStringLiteral("displayPage"));
  QCOMPARE(fixture.display()->refreshCount, 1);

  QVERIFY(fixture.object(QStringLiteral("displayOutputPreview")) != nullptr);
  QCOMPARE(fixture.object(QStringLiteral("displayOutputTitle"))
               ->property("text")
               .toString(),
           QStringLiteral("Studio Panel"));
  QCOMPARE(fixture.object(QStringLiteral("displayConnector"))
               ->property("text")
               .toString(),
           QStringLiteral("DP-1"));
  QVERIFY(fixture.object(QStringLiteral("displayResolutionRow_1920x1080")) !=
          nullptr);
  QVERIFY(fixture.object(QStringLiteral("displayNativeBadge_1920x1080"))
              ->property("visible")
              .toBool());
  QCOMPARE(fixture.object(QStringLiteral("displayResolutionRates_1920x1080"))
               ->property("text")
               .toString(),
           QStringLiteral("60 Hz · 165 Hz"));
  QCOMPARE(fixture.object(QStringLiteral("displayResolutionRates_1600x900"))
               ->property("text")
               .toString(),
           QStringLiteral("59.94 Hz"));
  QCOMPARE(fixture.object(QStringLiteral("displayRefreshRate"))
               ->property("text")
               .toString(),
           QStringLiteral("165 Hz"));
  QCOMPARE(fixture.object(QStringLiteral("displayScale"))
               ->property("text")
               .toString(),
           QStringLiteral("100%"));
  QCOMPARE(fixture.object(QStringLiteral("displayOrientation"))
               ->property("text")
               .toString(),
           QStringLiteral("Landscape"));
  QCOMPARE(fixture.object(QStringLiteral("displayVrrState"))
               ->property("text")
               .toString(),
           QStringLiteral("Supported"));
  QObject *wallpaper =
      fixture.object(QStringLiteral("displayWallpaperPreview"));
  QVERIFY(wallpaper != nullptr);
  QCOMPARE(wallpaper->property("source").toUrl(),
           QUrl(QStringLiteral(
               "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' "
               "width='2' height='2'%3E%3Crect width='2' height='2' "
               "fill='%234d72c9'/%3E%3C/svg%3E")));
}

void SettingsDisplayQmlTest::hostileOutputIdentityRemainsPlainText() {
  DisplayPageFixture fixture;
  QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.errors()));
  fixture.display()->setOutput(nativeOutput(QStringLiteral("<i>DP-1</i>"),
                                            QStringLiteral("<b>Monitor</b>")));
  QCoreApplication::processEvents();

  QObject *title = fixture.object(QStringLiteral("displayOutputTitle"));
  QObject *connector = fixture.object(QStringLiteral("displayConnector"));
  QVERIFY(title != nullptr);
  QVERIFY(connector != nullptr);
  QCOMPARE(title->property("text").toString(),
           QStringLiteral("<b>Monitor</b>"));
  QCOMPARE(connector->property("text").toString(),
           QStringLiteral("<i>DP-1</i>"));
  QCOMPARE(title->property("textFormat").toInt(), int(Qt::PlainText));
}

void SettingsDisplayQmlTest::
    disabledOutputWithoutCurrentModeUsesSafePlaceholder() {
  DisplayPageFixture fixture;
  QVariantMap output = nativeOutput();
  output.insert(QStringLiteral("enabled"), false);
  output.insert(QStringLiteral("hasCurrentMode"), false);
  output.insert(QStringLiteral("currentWidth"), 0);
  output.insert(QStringLiteral("currentHeight"), 0);
  output.insert(QStringLiteral("currentRefreshMilliHz"), 0);
  fixture.display()->setOutput(output);

  QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.errors()));
  QObject *preview = fixture.object(QStringLiteral("displayOutputPreview"));
  QObject *wallpaper =
      fixture.object(QStringLiteral("displayWallpaperPreview"));
  QVERIFY(preview != nullptr);
  QVERIFY(preview->property("width").toReal() > 0.0);
  QVERIFY(preview->property("height").toReal() > 0.0);
  QVERIFY(wallpaper != nullptr);
  QVERIFY(!wallpaper->property("visible").toBool());
  QCOMPARE(fixture.object(QStringLiteral("displayRefreshRate"))
               ->property("text")
               .toString(),
           QStringLiteral("Unavailable"));
}

QTEST_MAIN(SettingsDisplayQmlTest)
#include "SettingsDisplayQmlTest.moc"
