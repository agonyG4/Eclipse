#include "services/i18n/SettingsTranslationController.hpp"
#include "theme/ThemeController.hpp"
#include <QDir>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

namespace {

QVariantMap program(const QString &name) {
  return {{QStringLiteral("name"), name}, {QStringLiteral("origin"), QStringLiteral("system")}};
}

// Mirrors what a live Typhon reports: one system program without parameters,
// automatic blur, and explicit saturation and noise values.
class FakeVisualEffectsController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool available READ available CONSTANT)
  Q_PROPERTY(double materialPosition READ materialPosition NOTIFY snapshotChanged)
  Q_PROPERTY(double defaultMaterialPosition READ defaultMaterialPosition CONSTANT)
  Q_PROPERTY(double effectiveBlur READ blurValue NOTIFY snapshotChanged)
  Q_PROPERTY(double effectiveSaturation READ saturationValue NOTIFY snapshotChanged)
  Q_PROPERTY(double effectiveNoise READ noiseValue NOTIFY snapshotChanged)
  Q_PROPERTY(double blurValue READ blurValue NOTIFY snapshotChanged)
  Q_PROPERTY(double saturationValue READ saturationValue NOTIFY snapshotChanged)
  Q_PROPERTY(double noiseValue READ noiseValue NOTIFY snapshotChanged)
  Q_PROPERTY(bool blurOverrideSupported READ supported CONSTANT)
  Q_PROPERTY(bool saturationOverrideSupported READ supported CONSTANT)
  Q_PROPERTY(bool noiseOverrideSupported READ supported CONSTANT)
  Q_PROPERTY(bool blurOverridden READ blurOverridden NOTIFY snapshotChanged)
  Q_PROPERTY(bool saturationOverridden READ saturationOverridden NOTIFY snapshotChanged)
  Q_PROPERTY(bool noiseOverridden READ noiseOverridden NOTIFY snapshotChanged)
  Q_PROPERTY(bool hasOverrides READ hasOverrides NOTIFY snapshotChanged)
  Q_PROPERTY(QString lastError READ empty CONSTANT)
  Q_PROPERTY(bool materialProgramAvailable READ available CONSTANT)
  Q_PROPERTY(bool materialProgramRenderingAvailable READ available CONSTANT)
  Q_PROPERTY(bool materialProgramEditingAvailable READ available CONSTANT)
  Q_PROPERTY(QVariantList materialProgramCatalog READ catalog NOTIFY snapshotChanged)
  Q_PROPERTY(QString materialProgramRequested READ effectiveProgram CONSTANT)
  Q_PROPERTY(QString materialProgramPendingSelection READ empty CONSTANT)
  Q_PROPERTY(QString materialProgramEffective READ effectiveProgram CONSTANT)
  Q_PROPERTY(QString materialProgramFallbackReason READ empty CONSTANT)
  Q_PROPERTY(QString materialProgramEffectiveOrigin READ origin CONSTANT)
  Q_PROPERTY(QVariantList materialProgramParameters READ parameters CONSTANT)
  Q_PROPERTY(bool materialProgramHasOverrides READ falseValue CONSTANT)
  Q_PROPERTY(QString materialProgramError READ empty CONSTANT)

public:
  bool available() const { return true; }
  bool supported() const { return true; }
  bool falseValue() const { return false; }
  QString empty() const { return {}; }
  double materialPosition() const { return m_position; }
  double defaultMaterialPosition() const { return 0.5; }
  double blurValue() const { return m_blur.value_or(0.12); }
  double saturationValue() const { return m_saturation.value_or(1.0); }
  double noiseValue() const { return m_noise.value_or(0.0); }
  bool blurOverridden() const { return m_blur.has_value(); }
  bool saturationOverridden() const { return m_saturation.has_value(); }
  bool noiseOverridden() const { return m_noise.has_value(); }
  bool hasOverrides() const { return blurOverridden() || saturationOverridden() || noiseOverridden(); }
  QVariantList catalog() const { return m_catalog; }
  QString effectiveProgram() const { return QStringLiteral("system.background_blur"); }
  QString origin() const { return QStringLiteral("system"); }
  QVariantList parameters() const { return {}; }

  void setCatalog(const QVariantList &catalog) {
    m_catalog = catalog;
    emit snapshotChanged();
  }

  Q_INVOKABLE void refresh() {}
  Q_INVOKABLE void flush() { ++flushes; }
  Q_INVOKABLE void setMaterialPosition(double value) { change(m_position, value); }
  Q_INVOKABLE void setBlurOverride(double value) { change(m_blur, value); }
  Q_INVOKABLE void setSaturationOverride(double value) { change(m_saturation, value); }
  Q_INVOKABLE void setNoiseOverride(double value) { change(m_noise, value); }
  Q_INVOKABLE void clearBlurOverride() { change(m_blur, std::nullopt); }
  Q_INVOKABLE void clearSaturationOverride() { change(m_saturation, std::nullopt); }
  Q_INVOKABLE void clearNoiseOverride() { change(m_noise, std::nullopt); }
  Q_INVOKABLE void resetOverrides() {
    m_blur.reset();
    m_saturation.reset();
    m_noise.reset();
    emit snapshotChanged();
  }
  Q_INVOKABLE void restoreDefaults() {
    m_position = 0.5;
    resetOverrides();
  }
  Q_INVOKABLE void selectMaterialProgram(const QString &) {}
  Q_INVOKABLE void resetMaterialProgramParameters() {}

  int flushes = 0;

signals:
  void snapshotChanged();

private:
  template <typename T, typename V> void change(T &field, V value) {
    field = value;
    emit snapshotChanged();
  }

  double m_position = 0.0;
  std::optional<double> m_blur;
  std::optional<double> m_saturation = 1.0;
  std::optional<double> m_noise = 0.0;
  QVariantList m_catalog{program(QStringLiteral("system.background_blur"))};
};

class FakeWallpaperController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool busy READ busy CONSTANT)
  Q_PROPERTY(QUrl effectivePreviewUrl READ effectivePreviewUrl CONSTANT)
  Q_PROPERTY(QString effectiveFit READ effectiveFit CONSTANT)

public:
  bool busy() const { return false; }
  QUrl effectivePreviewUrl() const {
    return QUrl::fromLocalFile(qEnvironmentVariable("ASTREA_SETTINGS_TEST_WALLPAPER"));
  }
  QString effectiveFit() const { return QStringLiteral("cover"); }
  Q_INVOKABLE void refresh() {}
};

class FakeSettingsController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QObject *visualEffects READ visualEffects CONSTANT)
  Q_PROPERTY(QObject *wallpaper READ wallpaper CONSTANT)

public:
  QObject *visualEffects() { return &controller; }
  QObject *wallpaper() { return &wallpaperController; }
  FakeVisualEffectsController controller;
  FakeWallpaperController wallpaperController;
};

QObject *findItem(QQuickItem *item, const QString &name) {
  if (!item)
    return nullptr;
  if (item->objectName() == name)
    return item;
  for (QQuickItem *child : item->childItems()) {
    if (QObject *found = findItem(child, name))
      return found;
  }
  return nullptr;
}

class VisualEffectsPageFixture final {
public:
  VisualEffectsPageFixture() : m_theme(m_themeDir.filePath(QStringLiteral("theme.json"))) {
    m_engine.addImportPath(QStringLiteral("qrc:/"));
    m_engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"),
                                               &m_settings);
    m_engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &m_theme);
    m_engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &m_translations);
    QQmlComponent backdrop(&m_engine);
    backdrop.setData("import QtQuick\n"
                     "import \"qrc:/qt/qml/Astrea/Settings/qml/components\" as Components\n"
                     "Rectangle { anchors.fill: parent; color: Components.Theme.windowBackground }",
                     QUrl());
    m_background.reset(backdrop.create());
    if (auto *quick = qobject_cast<QQuickItem *>(m_background.get()))
      quick->setParentItem(m_window.contentItem());
    m_component = std::make_unique<QQmlComponent>(
        &m_engine,
        QUrl(QStringLiteral(
            "qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/VisualEffects.qml")));
    m_window.resize(900, 900);
  }

  ~VisualEffectsPageFixture() {
    if (m_page) {
      m_page->setParentItem(nullptr);
      delete m_page;
    }
  }

  QQuickItem *createPage() {
    QObject *created = m_component->create();
    m_page = qobject_cast<QQuickItem *>(created);
    if (!m_page) {
      delete created;
      return nullptr;
    }
    m_page->setParentItem(m_window.contentItem());
    m_page->setSize(m_window.size());
    if (auto *background = qobject_cast<QQuickItem *>(m_background.get()))
      background->setSize(m_window.size());
    m_window.show();
    QCoreApplication::processEvents();
    return m_page;
  }

  QString errors() const { return m_component->errorString(); }
  QObject *object(const QString &name) const { return findItem(m_page, name); }
  FakeVisualEffectsController &controller() { return m_settings.controller; }

  bool capture(const QString &name) {
    const QString directory = qEnvironmentVariable("ASTREA_SETTINGS_VISUAL_CAPTURE_DIR");
    if (directory.isEmpty())
      return true;
    QTest::qWait(300);
    return QDir().mkpath(directory) &&
           m_window.grabWindow().save(QDir(directory).filePath(name + ".png"));
  }

private:
  FakeSettingsController m_settings;
  QTemporaryDir m_themeDir;
  ThemeController m_theme;
  SettingsTranslationController m_translations;
  QQmlEngine m_engine;
  std::unique_ptr<QObject> m_background;
  std::unique_ptr<QQmlComponent> m_component;
  QQuickWindow m_window;
  QQuickItem *m_page = nullptr;
};

} // namespace

class SettingsVisualEffectsQmlTest final : public QObject {
  Q_OBJECT

private slots:
  void fineTuneExpandsIntoPerValueRows();
  void programSectionAppearsOnlyWithAChoice();
};

void SettingsVisualEffectsQmlTest::fineTuneExpandsIntoPerValueRows() {
  VisualEffectsPageFixture fixture;
  QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.errors()));
  QVERIFY(fixture.capture(QStringLiteral("visual-effects-collapsed")));

  QObject *toggle = fixture.object(QStringLiteral("visualEffectsAdvancedToggle"));
  QObject *panel = fixture.object(QStringLiteral("visualEffectsAdvancedCard"));
  QVERIFY(toggle != nullptr);
  QVERIFY(panel != nullptr);
  QVERIFY(toggle->property("visible").toBool());
  QVERIFY(!panel->property("visible").toBool());

  QVERIFY(QMetaObject::invokeMethod(toggle, "clicked"));
  QTRY_VERIFY(panel->property("visible").toBool());
  QVERIFY(fixture.capture(QStringLiteral("visual-effects-expanded")));

  QObject *blur = fixture.object(QStringLiteral("materialBlurSlider"));
  QVERIFY(blur != nullptr);
  QVERIFY(QMetaObject::invokeMethod(blur, "valueEdited", Q_ARG(qreal, 0.4)));
  QVERIFY(fixture.controller().blurOverridden());
  QCOMPARE(fixture.controller().blurValue(), 0.4);

  QObject *reset = fixture.object(QStringLiteral("resetAdvancedCustomizations"));
  QVERIFY(reset != nullptr);
  QVERIFY(reset->property("enabled").toBool());
  QVERIFY(QMetaObject::invokeMethod(reset, "clicked"));
  QVERIFY(!fixture.controller().hasOverrides());
  QTRY_VERIFY(!reset->property("enabled").toBool());
}

void SettingsVisualEffectsQmlTest::programSectionAppearsOnlyWithAChoice() {
  VisualEffectsPageFixture fixture;
  QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.errors()));

  QObject *card = fixture.object(QStringLiteral("materialProgramCard"));
  QVERIFY(card != nullptr);
  // One program without parameters leaves nothing to pick.
  QVERIFY(!card->property("visible").toBool());

  fixture.controller().setCatalog({program(QStringLiteral("system.background_blur")),
                                   program(QStringLiteral("studio.soft_glow"))});
  QTRY_VERIFY(card->property("visible").toBool());
  QObject *selector = fixture.object(QStringLiteral("materialProgramSelector"));
  QVERIFY(selector != nullptr);
  QVERIFY(selector->property("enabled").toBool());
  QCOMPARE(selector->property("label").toString(), QStringLiteral("Background Blur"));
}

QTEST_MAIN(SettingsVisualEffectsQmlTest)
#include "SettingsVisualEffectsQmlTest.moc"
