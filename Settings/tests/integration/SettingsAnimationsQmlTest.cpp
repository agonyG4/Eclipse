#include "services/i18n/SettingsTranslationController.hpp"
#include "theme/ThemeController.hpp"
#include <QDir>
#include <QTemporaryDir>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QUrl>
#include <QtTest>

namespace {

QVariantMap slotCapability(const QString &id, const QStringList &effects,
                           const QString &effective, double speed = 1.0) {
  return {
      {QStringLiteral("id"), id},
      {QStringLiteral("compatibleEffects"), effects},
      {QStringLiteral("availableEffects"), effects},
      {QStringLiteral("plannedEffects"), QStringList{}},
      {QStringLiteral("unavailableEffects"), QStringList{}},
      {QStringLiteral("requested"), effective},
      {QStringLiteral("effective"), effective},
      {QStringLiteral("speed"), speed},
  };
}

class FakeAnimationController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool available READ available CONSTANT)
  Q_PROPERTY(bool busy READ busy CONSTANT)
  Q_PROPERTY(bool enabled READ enabled CONSTANT)
  Q_PROPERTY(bool animateMaximizedWindowOpen READ animateMaximizedWindowOpen CONSTANT)
  Q_PROPERTY(QString preset READ preset CONSTANT)
  Q_PROPERTY(double speed READ speed NOTIFY snapshotChanged)
  Q_PROPERTY(bool hasOverrides READ hasOverrides NOTIFY snapshotChanged)
  Q_PROPERTY(QVariantList slots READ slotList NOTIFY snapshotChanged)
  Q_PROPERTY(QVariantList presets READ presets CONSTANT)
  Q_PROPERTY(QString lastError READ lastError CONSTANT)

public:
  FakeAnimationController() {
    const QStringList geometry{QStringLiteral("none"), QStringLiteral("geometry.kde"),
                               QStringLiteral("geometry.macos"),
                               QStringLiteral("geometry.hyprland")};
    const QStringList window{QStringLiteral("none"), QStringLiteral("window.scale"),
                             QStringLiteral("window.glide"), QStringLiteral("window.popin")};
    const QStringList lifecycle{QStringLiteral("none"), QStringLiteral("minimize.lamp"),
                                QStringLiteral("minimize.squash")};
    m_slots = {
        slotCapability(QStringLiteral("window.open"), window, QStringLiteral("window.popin")),
        slotCapability(QStringLiteral("window.close"), window, QStringLiteral("window.popin"), 1.5),
        slotCapability(QStringLiteral("window.minimize"), lifecycle, QStringLiteral("minimize.lamp")),
        slotCapability(QStringLiteral("window.restore"), lifecycle, QStringLiteral("minimize.lamp")),
        slotCapability(QStringLiteral("window.move"), geometry, QStringLiteral("geometry.hyprland")),
        slotCapability(QStringLiteral("window.resize"), geometry, QStringLiteral("geometry.hyprland")),
        slotCapability(QStringLiteral("layout.reflow"), geometry, QStringLiteral("geometry.hyprland")),
        slotCapability(QStringLiteral("window.maximize"), geometry, QStringLiteral("geometry.hyprland")),
        slotCapability(QStringLiteral("window.fullscreen"), geometry, QStringLiteral("none")),
        slotCapability(QStringLiteral("workspace.switch"),
                       {QStringLiteral("none"), QStringLiteral("workspace.slide"),
                        QStringLiteral("workspace.slide.hyprland")},
                       QStringLiteral("workspace.slide.hyprland"), 0.75),
        slotCapability(QStringLiteral("workspace.special"),
                       {QStringLiteral("none"), QStringLiteral("special.slide"),
                        QStringLiteral("special.slide.hyprland")},
                       QStringLiteral("special.slide.hyprland")),
        slotCapability(QStringLiteral("workspace.window-move"), {QStringLiteral("none")},
                       QStringLiteral("none")),
    };
  }

  bool available() const { return true; }
  bool busy() const { return false; }
  bool enabled() const { return true; }
  bool animateMaximizedWindowOpen() const { return true; }
  QString preset() const { return QStringLiteral("hyprland"); }
  double speed() const { return m_speed; }
  bool hasOverrides() const { return true; }
  QVariantList slotList() const { return m_slots; }
  QVariantList presets() const {
    return {QStringLiteral("astrea"), QStringLiteral("kde"), QStringLiteral("macos"),
            QStringLiteral("hyprland")};
  }
  QString lastError() const { return {}; }

  Q_INVOKABLE void refresh() {}
  Q_INVOKABLE void setEnabled(bool) {}
  Q_INVOKABLE void setAnimateMaximizedWindowOpen(bool) {}
  Q_INVOKABLE void setPreset(const QString &) {}
  Q_INVOKABLE void setSpeed(double value) {
    m_speed = value;
    emit snapshotChanged();
  }
  Q_INVOKABLE void setSlotEffect(const QString &, const QString &) {}
  Q_INVOKABLE void setSlotSpeed(const QString &slotId, double value) {
    lastSlotSpeed = {slotId, value};
    for (QVariant &slot : m_slots) {
      QVariantMap map = slot.toMap();
      if (map.value(QStringLiteral("id")).toString() == slotId) {
        map.insert(QStringLiteral("speed"), value);
        slot = map;
      }
    }
    emit snapshotChanged();
  }
  Q_INVOKABLE void clearSlotOverride(const QString &) {}
  Q_INVOKABLE void resetOverrides() {}
  Q_INVOKABLE void restoreDefaults() {}
  Q_INVOKABLE void flush() { ++flushCount; }

  QPair<QString, double> lastSlotSpeed;
  int flushCount = 0;

signals:
  void snapshotChanged();

private:
  double m_speed = 1.0;
  QVariantList m_slots;
};

class FakeSettingsController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QObject *animations READ animations CONSTANT)

public:
  QObject *animations() { return &controller; }
  FakeAnimationController controller;
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

class AnimationsPageFixture final {
public:
  AnimationsPageFixture() : m_theme(m_themeDir.filePath(QStringLiteral("theme.json"))) {
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
            "qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Animations.qml")));
    m_window.resize(900, 1500);
  }

  ~AnimationsPageFixture() {
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
  // Repeater delegates are visual children only, so walk the item tree.
  QObject *object(const QString &name) const { return findItem(m_page, name); }
  FakeAnimationController &controller() { return m_settings.controller; }

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

class SettingsAnimationsQmlTest final : public QObject {
  Q_OBJECT

private slots:
  void slotRowExpandsIntoASteppedSpeedControl();
};

void SettingsAnimationsQmlTest::slotRowExpandsIntoASteppedSpeedControl() {
  AnimationsPageFixture fixture;
  QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.errors()));
  QVERIFY(fixture.capture(QStringLiteral("animations-collapsed")));

  QObject *row = fixture.object(QStringLiteral("animationSlotRow_workspace.switch"));
  QObject *panel = fixture.object(QStringLiteral("animationSpeedPanel_workspace.switch"));
  QVERIFY(row != nullptr);
  QVERIFY(panel != nullptr);
  QCOMPARE(panel->property("implicitHeight").toReal(), 0.0);

  QVERIFY(QMetaObject::invokeMethod(row, "toggle"));
  QTRY_VERIFY(panel->property("implicitHeight").toReal() > 40.0);
  QVERIFY(fixture.capture(QStringLiteral("animations-expanded")));

  QObject *slider = fixture.object(QStringLiteral("animationSpeedSlider_workspace.switch"));
  QVERIFY(slider != nullptr);
  // 0.75x is the third stop.
  QCOMPARE(slider->property("value").toReal(), 2.0);
  QVERIFY(QMetaObject::invokeMethod(slider, "valueEdited", Q_ARG(qreal, 6.0)));
  QCOMPARE(fixture.controller().lastSlotSpeed.first, QStringLiteral("workspace.switch"));
  QCOMPARE(fixture.controller().lastSlotSpeed.second, 2.0);

  // A row without an animation does not expand.
  QObject *none = fixture.object(QStringLiteral("animationSlotRow_window.fullscreen"));
  QVERIFY(none != nullptr);
  QVERIFY(!none->property("animated").toBool());
}

QTEST_MAIN(SettingsAnimationsQmlTest)
#include "SettingsAnimationsQmlTest.moc"
