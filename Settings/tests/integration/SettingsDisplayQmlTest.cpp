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
  Q_PROPERTY(bool available READ available NOTIFY statusChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
  Q_PROPERTY(QVariantList outputs READ outputs NOTIFY snapshotChanged)
  Q_PROPERTY(quint32 outputCount READ outputCount NOTIFY snapshotChanged)
  Q_PROPERTY(bool truncated READ truncated NOTIFY snapshotChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)

public:
  bool available() const { return m_available; }
  bool busy() const { return m_busy; }
  QVariantList outputs() const { return m_outputs; }
  quint32 outputCount() const { return quint32(m_outputs.size()); }
  bool truncated() const { return m_truncated; }
  QString lastError() const { return m_lastError; }
  Q_INVOKABLE void refresh() { ++refreshCount; }
  Q_INVOKABLE bool setDraft(const QString &, quint32, quint32,
                            const QString &) {
    ++setDraftCount;
    return true;
  }
  Q_INVOKABLE void applyDraft(const QString &) { ++applyDraftCount; }
  Q_INVOKABLE void confirm() { ++confirmCount; }
  Q_INVOKABLE void revert() { ++revertCount; }

  void setOutput(const QVariantMap &output) {
    setOutputs({output});
  }

  void setOutputs(const QVariantList &outputs) {
    m_outputs = outputs;
    emit snapshotChanged();
  }

  void setAvailable(bool available) {
    if (m_available == available)
      return;
    m_available = available;
    emit statusChanged();
  }

  void setBusy(bool busy) {
    if (m_busy == busy)
      return;
    m_busy = busy;
    emit statusChanged();
  }

  void setLastError(const QString &error) {
    if (m_lastError == error)
      return;
    m_lastError = error;
    emit errorChanged();
  }

  void setTruncated(bool truncated) {
    if (m_truncated == truncated)
      return;
    m_truncated = truncated;
    emit snapshotChanged();
  }

  int refreshCount = 0;

signals:
  void statusChanged();
  void snapshotChanged();
  void errorChanged();

private:
  QVariantList m_outputs;
  bool m_available = true;
  bool m_busy = false;
  bool m_truncated = false;
  QString m_lastError;

public:
  int applyDraftCount = 0;
  int confirmCount = 0;
  int revertCount = 0;
  int setDraftCount = 0;
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
displayOutput(const QString &name = QStringLiteral("DP-1"),
              const QString &model = QStringLiteral("Studio Panel")) {
  const QVariantList modes{
      QVariantMap{{QStringLiteral("width"), 1920},
                  {QStringLiteral("height"), 1080},
                  {QStringLiteral("id"), 10},
                  {QStringLiteral("refreshMilliHz"), 60000},
                  {QStringLiteral("preferred"), true},
                  {QStringLiteral("interlaced"), false}},
      QVariantMap{{QStringLiteral("width"), 1920},
                  {QStringLiteral("height"), 1080},
                  {QStringLiteral("id"), 11},
                  {QStringLiteral("refreshMilliHz"), 165000},
                  {QStringLiteral("preferred"), false},
                  {QStringLiteral("interlaced"), false}},
      QVariantMap{{QStringLiteral("width"), 1600},
                  {QStringLiteral("height"), 900},
                  {QStringLiteral("id"), 12},
                  {QStringLiteral("refreshMilliHz"), 59940},
                  {QStringLiteral("preferred"), false},
                  {QStringLiteral("interlaced"), false}},
      QVariantMap{{QStringLiteral("width"), 1920},
                  {QStringLiteral("height"), 1080},
                  {QStringLiteral("id"), 13},
                  {QStringLiteral("refreshMilliHz"), 60000},
                  {QStringLiteral("preferred"), false},
                  {QStringLiteral("interlaced"), true}},
      QVariantMap{{QStringLiteral("width"), 1366},
                  {QStringLiteral("height"), 768},
                  {QStringLiteral("id"), 14},
                  {QStringLiteral("refreshMilliHz"), 60000},
                  {QStringLiteral("preferred"), false},
                  {QStringLiteral("interlaced"), false}},
      QVariantMap{{QStringLiteral("width"), 1280},
                  {QStringLiteral("height"), 720},
                  {QStringLiteral("id"), 15},
                  {QStringLiteral("refreshMilliHz"), 60000},
                  {QStringLiteral("preferred"), false},
                  {QStringLiteral("interlaced"), false}},
      QVariantMap{{QStringLiteral("width"), 1024},
                  {QStringLiteral("height"), 768},
                  {QStringLiteral("id"), 16},
                  {QStringLiteral("refreshMilliHz"), 60000},
                  {QStringLiteral("preferred"), false},
                  {QStringLiteral("interlaced"), false}},
      QVariantMap{{QStringLiteral("width"), 800},
                  {QStringLiteral("height"), 600},
                  {QStringLiteral("id"), 17},
                  {QStringLiteral("refreshMilliHz"), 60000},
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
          {QStringLiteral("currentModeId"), 11},
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

void clickItem(QQuickItem *item) {
  if (!item || !item->window())
    return;
  const QPoint position =
      item->mapToScene(item->boundingRect().center()).toPoint();
  QTest::mouseClick(item->window(), Qt::LeftButton, Qt::NoModifier, position);
}

class DisplayPageFixture final {
public:
  DisplayPageFixture() {
    m_settings.display()->setOutput(displayOutput());
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
  void restoresHistoricalDisplayHierarchyAndCompactRows();
  void showsPreferredModeSemantics();
  void communicatesOutputListTruncation();
  void preservesPerOutputModeTruncationNotice();
  void hostileOutputIdentityRemainsPlainText();
  void disabledOutputWithoutCurrentModeUsesSafePlaceholder();
  void expandsAndCollapsesResolutionList();
  void monitorSelectorIsConditionalAndUsesOutputIds();
  void unavailableBackendCanBeRetried();
  void failedRefreshCanBeRetried();
  void showsEmptyOutputState();
  void showsLoadingStateUntilRefreshSettles();
  void builtInDisplaysUseLaptopChassis();
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
  QObject *preferredBadge =
      fixture.object(QStringLiteral("displayPreferredBadge_1920x1080"));
  QVERIFY(preferredBadge != nullptr);
  QVERIFY(preferredBadge->property("visible").toBool());
  QObject *preferredLabel =
      fixture.object(QStringLiteral("displayPreferredLabel_1920x1080"));
  QVERIFY(preferredLabel != nullptr);
  QCOMPARE(preferredLabel->property("text").toString(),
           QStringLiteral("Preferred"));
  QCOMPARE(fixture.object(QStringLiteral("displayRefreshControl"))
               ->property("text")
               .toString(),
           QStringLiteral("165 Hz"));
  QCOMPARE(fixture.object(QStringLiteral("displayScaleControl"))
               ->property("text")
               .toString(),
           QStringLiteral("100%"));
  QCOMPARE(fixture.object(QStringLiteral("displayVrrControl"))
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

void SettingsDisplayQmlTest::restoresHistoricalDisplayHierarchyAndCompactRows() {
  DisplayPageFixture fixture;
  QQuickItem *page = fixture.createPage();
  QVERIFY2(page != nullptr, qPrintable(fixture.errors()));

  QObject *previewHeader =
      fixture.object(QStringLiteral("displayPreviewSectionHeader"));
  QVERIFY(previewHeader != nullptr);
  QCOMPARE(previewHeader->property("text").toString(),
           QStringLiteral("DISPLAY PREVIEW"));

  QQuickItem *preview =
      qobject_cast<QQuickItem *>(fixture.object(QStringLiteral("displayOutputPreview")));
  QQuickItem *identity =
      qobject_cast<QQuickItem *>(fixture.object(QStringLiteral("displayPreviewIdentity")));
  QQuickItem *previewOpenArea = qobject_cast<QQuickItem *>(
      fixture.object(QStringLiteral("displayPreviewOpenArea")));
  QVERIFY(preview != nullptr);
  QVERIFY(identity != nullptr);
  QVERIFY(previewOpenArea != nullptr);
  QVERIFY(preview->width() > 0.0);
  QVERIFY(preview->property("height").toReal() <= 260.0);
  QVERIFY(qAbs(previewOpenArea->implicitHeight() - preview->height()
               - identity->implicitHeight() - 38.0) < 0.5);
  QTRY_VERIFY(identity->mapToItem(page, QPointF(0, 0)).y() >
              preview->mapToItem(page, QPointF(0, 0)).y());
  QObject *title = fixture.object(QStringLiteral("displayOutputTitle"));
  QObject *subtitle = fixture.object(QStringLiteral("displayOutputSubtitle"));
  QVERIFY(title != nullptr);
  QVERIFY(subtitle != nullptr);
  QCOMPARE(title->property("textFormat").toInt(), int(Qt::PlainText));
  QCOMPARE(subtitle->property("text").toString(),
           QStringLiteral("External display · Astrea Labs · DP-1"));
  QCOMPARE(fixture.object(QStringLiteral("displayModeSummary"))
               ->property("text")
               .toString(),
           QStringLiteral("1920 × 1080 • 165 Hz"));
  QObject *previewMonitor =
      fixture.object(QStringLiteral("displayPreviewMonitor_output-1"));
  QVERIFY(previewMonitor != nullptr);
  QVERIFY(previewMonitor->property("active").toBool());
  QVERIFY(fixture.object(QStringLiteral("displayMonitorStand_output-1"))
              ->property("visible")
              .toBool());

  QObject *monitorSection =
      fixture.object(QStringLiteral("displayMonitorSection"));
  QVERIFY(monitorSection != nullptr);
  QVERIFY(!monitorSection->property("visible").toBool());

  QObject *resolutionHeader =
      fixture.object(QStringLiteral("displayResolutionSectionHeader"));
  QVERIFY(resolutionHeader != nullptr);
  QCOMPARE(resolutionHeader->property("text").toString(),
           QStringLiteral("RESOLUTION"));
  QObject *resolutionRow =
      fixture.object(QStringLiteral("displayResolutionRow_1920x1080"));
  QVERIFY(resolutionRow != nullptr);
  QVERIFY(resolutionRow->property("selected").toBool());
  QCOMPARE(resolutionRow->property("implicitHeight").toReal(), 36.0);
  QVERIFY(fixture.object(QStringLiteral("displayResolutionRates_1920x1080")) ==
          nullptr);

  QObject *settingsCard =
      fixture.object(QStringLiteral("displaySettingsCard"));
  QObject *refreshRow =
      fixture.object(QStringLiteral("displaySettingRefresh"));
  QObject *colorDepthRow =
      fixture.object(QStringLiteral("displaySettingColorDepth"));
  QObject *vrrRow = fixture.object(QStringLiteral("displaySettingVrr"));
  QObject *scaleRow = fixture.object(QStringLiteral("displaySettingScale"));
  QVERIFY(settingsCard != nullptr);
  QVERIFY(refreshRow != nullptr);
  QVERIFY(colorDepthRow != nullptr);
  QVERIFY(vrrRow != nullptr);
  QVERIFY(scaleRow != nullptr);
  auto pageY = [page](QObject *object) {
    return qobject_cast<QQuickItem *>(object)
        ->mapToItem(page, QPointF(0, 0))
        .y();
  };
  QVERIFY(pageY(refreshRow) < pageY(colorDepthRow));
  QVERIFY(pageY(colorDepthRow) < pageY(vrrRow));
  QVERIFY(pageY(vrrRow) < pageY(scaleRow));

  QObject *refreshControl =
      fixture.object(QStringLiteral("displayRefreshControl"));
  QVERIFY(refreshControl != nullptr);
  QCOMPARE(refreshControl->property("text").toString(),
           QStringLiteral("165 Hz"));
  QVERIFY(!refreshControl->property("enabled").toBool());
  const QVariantList refreshModes =
      refreshControl->property("options").toList();
  QCOMPARE(refreshModes.size(), 3);
  QCOMPARE(refreshModes.at(0).toMap().value(QStringLiteral("id")).toUInt(),
           10U);
  QCOMPARE(refreshModes.at(1).toMap().value(QStringLiteral("id")).toUInt(),
           13U);
  QVERIFY(refreshModes.at(1).toMap().value(QStringLiteral("interlaced")).toBool());
  QCOMPARE(refreshControl->property("selectedModeId").toUInt(), 11U);

  QObject *vrrControl = fixture.object(QStringLiteral("displayVrrControl"));
  QObject *colorDepthControl =
      fixture.object(QStringLiteral("displayColorDepthControl"));
  QObject *scaleControl = fixture.object(QStringLiteral("displayScaleControl"));
  QVERIFY(vrrControl != nullptr);
  QVERIFY(colorDepthControl != nullptr);
  QVERIFY(scaleControl != nullptr);
  QVERIFY(!vrrControl->property("enabled").toBool());
  QVERIFY(!colorDepthControl->property("enabled").toBool());
  QVERIFY(!scaleControl->property("enabled").toBool());
  QCOMPARE(vrrControl->property("text").toString(), QStringLiteral("Supported"));
  QCOMPARE(scaleControl->property("text").toString(), QStringLiteral("100%"));

  QObject *apply = fixture.object(QStringLiteral("displayApplyButton"));
  QVERIFY(apply != nullptr);
  QVERIFY(!apply->property("enabled").toBool());
  QObject *nightShift =
      fixture.object(QStringLiteral("displayNightShiftControl"));
  QVERIFY(nightShift != nullptr);
  QVERIFY(!nightShift->property("enabled").toBool());
  clickItem(qobject_cast<QQuickItem *>(refreshControl));
  clickItem(qobject_cast<QQuickItem *>(vrrControl));
  clickItem(qobject_cast<QQuickItem *>(scaleControl));
  clickItem(qobject_cast<QQuickItem *>(apply));
  clickItem(qobject_cast<QQuickItem *>(nightShift));
  QCOMPARE(fixture.display()->setDraftCount, 0);
  QCOMPARE(fixture.display()->applyDraftCount, 0);
  QCOMPARE(fixture.display()->confirmCount, 0);
  QCOMPARE(fixture.display()->revertCount, 0);
  QObject *appliedFeedback =
      fixture.object(QStringLiteral("displayAppliedFeedback"));
  QVERIFY(appliedFeedback != nullptr);
  QVERIFY(!appliedFeedback->property("visible").toBool());
}

void SettingsDisplayQmlTest::expandsAndCollapsesResolutionList() {
  DisplayPageFixture fixture;
  QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.errors()));

  QObject *toggle =
      fixture.object(QStringLiteral("displayResolutionListToggle"));
  QVERIFY(toggle != nullptr);
  QVERIFY(fixture.object(QStringLiteral("displayResolutionRow_800x600")) ==
          nullptr);
  QVERIFY(fixture.object(QStringLiteral("displayResolutionRow_1920x1080")) !=
          nullptr);
  clickItem(qobject_cast<QQuickItem *>(toggle));
  QTRY_VERIFY(fixture.object(QStringLiteral("displayResolutionRow_800x600")) !=
              nullptr);
  QCOMPARE(fixture.object(QStringLiteral("displayResolutionListToggleLabel"))
               ->property("text")
               .toString(),
           QStringLiteral("⌃  Show less"));

  clickItem(qobject_cast<QQuickItem *>(toggle));
  QTRY_VERIFY(fixture.object(QStringLiteral("displayResolutionRow_800x600")) ==
              nullptr);
}

void SettingsDisplayQmlTest::monitorSelectorIsConditionalAndUsesOutputIds() {
  DisplayPageFixture fixture;
  QVariantMap second = displayOutput(QStringLiteral("HDMI-1"),
                                     QStringLiteral("Conference Monitor"));
  second.insert(QStringLiteral("id"), QStringLiteral("output-2"));
  second.insert(QStringLiteral("x"), 1920);
  second.insert(QStringLiteral("focused"), true);
  QVariantMap first = displayOutput();
  first.insert(QStringLiteral("focused"), false);
  fixture.display()->setOutputs({first, second});
  QQuickItem *page = fixture.createPage();
  QVERIFY2(page != nullptr, qPrintable(fixture.errors()));

  QObject *monitorSection =
      fixture.object(QStringLiteral("displayMonitorSection"));
  QObject *selector =
      fixture.object(QStringLiteral("displayMonitorSelector"));
  QVERIFY(monitorSection != nullptr);
  QVERIFY(monitorSection->property("visible").toBool());
  QVERIFY(selector != nullptr);
  QCOMPARE(page->property("selectedOutputId").toString(),
           QStringLiteral("output-2"));
  QCOMPARE(selector->property("currentIndex").toInt(), 1);
  QQuickItem *firstPreview = qobject_cast<QQuickItem *>(
      fixture.object(QStringLiteral("displayPreviewMonitor_output-1")));
  QQuickItem *secondPreview = qobject_cast<QQuickItem *>(
      fixture.object(QStringLiteral("displayPreviewMonitor_output-2")));
  QVERIFY(firstPreview != nullptr);
  QVERIFY(secondPreview != nullptr);
  QTRY_VERIFY_WITH_TIMEOUT(secondPreview->x() > firstPreview->x(), 1500);
  QCOMPARE(fixture.object(QStringLiteral("displayOutputTitle"))
               ->property("text")
               .toString(),
           QStringLiteral("Conference Monitor"));
}

void SettingsDisplayQmlTest::unavailableBackendCanBeRetried() {
  DisplayPageFixture fixture;
  fixture.display()->setAvailable(false);
  QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.errors()));

  QObject *unavailable =
      fixture.object(QStringLiteral("displayBackendUnavailable"));
  QObject *retry = fixture.object(QStringLiteral("displayRetryButton"));
  QVERIFY(unavailable != nullptr);
  QVERIFY(unavailable->property("visible").toBool());
  QVERIFY(retry != nullptr);
  QVERIFY(retry->property("enabled").toBool());
  QCOMPARE(fixture.display()->refreshCount, 1);
  clickItem(qobject_cast<QQuickItem *>(retry));
  QCOMPARE(fixture.display()->refreshCount, 2);
}

void SettingsDisplayQmlTest::failedRefreshCanBeRetried() {
  DisplayPageFixture fixture;
  fixture.display()->setLastError(QStringLiteral("Refresh failed"));
  QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.errors()));

  QObject *error = fixture.object(QStringLiteral("displayRefreshError"));
  QObject *retry = fixture.object(QStringLiteral("displayRetryButton"));
  QVERIFY(error != nullptr);
  QVERIFY(error->property("visible").toBool());
  QCOMPARE(error->property("text").toString(),
           QStringLiteral("Refresh failed"));
  QVERIFY(retry != nullptr);
  clickItem(qobject_cast<QQuickItem *>(retry));
  QCOMPARE(fixture.display()->refreshCount, 2);
}

void SettingsDisplayQmlTest::showsEmptyOutputState() {
  DisplayPageFixture fixture;
  fixture.display()->setOutputs({});
  QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.errors()));

  QObject *empty = fixture.object(QStringLiteral("displayNoOutputs"));
  QVERIFY(empty != nullptr);
  QVERIFY(empty->property("visible").toBool());
  QObject *content = fixture.object(QStringLiteral("displayContent"));
  QVERIFY(content != nullptr);
  QVERIFY(!content->property("visible").toBool());
}

void SettingsDisplayQmlTest::showsLoadingStateUntilRefreshSettles() {
  DisplayPageFixture fixture;
  fixture.display()->setOutputs({});
  fixture.display()->setBusy(true);
  QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.errors()));

  QObject *loading = fixture.object(QStringLiteral("displayLoading"));
  QObject *empty = fixture.object(QStringLiteral("displayNoOutputs"));
  QVERIFY(loading != nullptr);
  QVERIFY(empty != nullptr);
  QVERIFY(loading->property("visible").toBool());
  QVERIFY(!empty->property("visible").toBool());

  fixture.display()->setBusy(false);
  QTRY_VERIFY(empty->property("visible").toBool());
  QVERIFY(!loading->property("visible").toBool());
}

void SettingsDisplayQmlTest::builtInDisplaysUseLaptopChassis() {
  DisplayPageFixture fixture;
  fixture.display()->setOutput(displayOutput(QStringLiteral("eDP-1"),
                                             QStringLiteral("Laptop panel")));
  QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.errors()));

  QObject *preview =
      fixture.object(QStringLiteral("displayPreviewMonitor_output-1"));
  QObject *chassis =
      fixture.object(QStringLiteral("displayLaptopChassis_output-1"));
  QObject *externalStand =
      fixture.object(QStringLiteral("displayMonitorStand_output-1"));
  QVERIFY(preview != nullptr);
  QVERIFY(preview->property("laptop").toBool());
  QVERIFY(chassis != nullptr);
  QVERIFY(chassis->property("visible").toBool());
  QVERIFY(externalStand != nullptr);
  QVERIFY(!externalStand->property("visible").toBool());
}

void SettingsDisplayQmlTest::showsPreferredModeSemantics() {
  DisplayPageFixture fixture;
  QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.errors()));

  QObject *preferredBadge =
      fixture.object(QStringLiteral("displayPreferredBadge_1920x1080"));
  QVERIFY(preferredBadge != nullptr);
  QVERIFY(preferredBadge->property("visible").toBool());
  QObject *preferredLabel =
      fixture.object(QStringLiteral("displayPreferredLabel_1920x1080"));
  QVERIFY(preferredLabel != nullptr);
  QCOMPARE(preferredLabel->property("text").toString(),
           QStringLiteral("Preferred"));
  QVERIFY(fixture.object(QStringLiteral("displayNativeBadge_1920x1080")) ==
          nullptr);
}

void SettingsDisplayQmlTest::communicatesOutputListTruncation() {
  DisplayPageFixture fixture;
  QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.errors()));

  QObject *notice = fixture.object(QStringLiteral("displayOutputsTruncated"));
  QVERIFY(notice != nullptr);
  QVERIFY(!notice->property("visible").toBool());

  fixture.display()->setTruncated(true);
  QTRY_VERIFY(notice->property("visible").toBool());
  QCOMPARE(notice->property("text").toString(),
           QStringLiteral("Additional displays are not shown."));
}

void SettingsDisplayQmlTest::preservesPerOutputModeTruncationNotice() {
  DisplayPageFixture fixture;
  QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.errors()));

  QObject *notice = fixture.object(QStringLiteral("displayModesTruncated"));
  QVERIFY(notice != nullptr);
  QVERIFY(!notice->property("visible").toBool());

  QVariantMap output = displayOutput();
  output.insert(QStringLiteral("modesTruncated"), true);
  fixture.display()->setOutput(output);
  QTRY_VERIFY(notice->property("visible").toBool());
  QCOMPARE(notice->property("text").toString(),
           QStringLiteral("Some available modes are not shown."));
}

void SettingsDisplayQmlTest::hostileOutputIdentityRemainsPlainText() {
  DisplayPageFixture fixture;
  QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.errors()));
  fixture.display()->setOutput(displayOutput(QStringLiteral("<i>DP-1</i>"),
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
  QVariantMap output = displayOutput();
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
  QCOMPARE(fixture.object(QStringLiteral("displayRefreshControl"))
               ->property("text")
               .toString(),
           QStringLiteral("Unavailable"));
}

QTEST_MAIN(SettingsDisplayQmlTest)
#include "SettingsDisplayQmlTest.moc"
