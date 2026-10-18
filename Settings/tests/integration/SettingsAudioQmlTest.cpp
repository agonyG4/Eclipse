#include "core/SettingsController.hpp"
#include "system/SystemServiceState.hpp"
#include "system/audio/AudioBackend.hpp"
#include "system/audio/AudioOutputModel.hpp"
#include "system/audio/AudioService.hpp"
#include "theme/ThemeController.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSaveFile>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtQml>
#include <QtTest>

#include <memory>

namespace {

class FakeI18n final : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE QString tr(const QString &, const QString &fallback) const { return fallback; }
};

class NullAudioController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *audio READ audio CONSTANT)
    Q_PROPERTY(QObject *spatialAudio READ spatialAudio CONSTANT)

public:
    QObject *audio() const { return nullptr; }
    QObject *spatialAudio() const { return nullptr; }
};

class XdgConfigHome final {
public:
    explicit XdgConfigHome(const QString &path)
        : m_wasSet(qEnvironmentVariableIsSet("XDG_CONFIG_HOME"))
        , m_previous(qgetenv("XDG_CONFIG_HOME"))
    {
        qputenv("XDG_CONFIG_HOME", path.toUtf8());
    }

    ~XdgConfigHome()
    {
        if (m_wasSet)
            qputenv("XDG_CONFIG_HOME", m_previous);
        else
            qunsetenv("XDG_CONFIG_HOME");
    }

private:
    bool m_wasSet = false;
    QByteArray m_previous;
};

QString spatialAudioConfigPath(const QString &configHome)
{
    return QDir(configHome).filePath(QStringLiteral("AstreaOS/audio/spatial.json"));
}

bool writeConfigAtomically(const QString &path, const QByteArray &contents)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size()
        && file.commit();
}

class PageAudioBackend final : public Astrea::System::AudioBackend {
public:
    bool start(const Callbacks &callbacks, QString *errorOut) override
    {
        m_callbacks = callbacks;
        ++startCount;
        if (!startResult && errorOut)
            *errorOut = startupError;
        return startResult;
    }

    void stop() override { ++stopCount; }

    bool setDefaultOutput(quint32 nodeId) override
    {
        defaultOutputRequests.append(nodeId);
        return actionResult;
    }

    bool setVolume(quint32 nodeId, double linear) override
    {
        ++setVolumeCount;
        volumeNodeId = nodeId;
        volumeLinear = linear;
        return actionResult;
    }

    bool setMute(quint32 nodeId, bool muted) override
    {
        ++setMuteCount;
        muteNodeId = nodeId;
        muteValue = muted;
        return actionResult;
    }

    bool setDefaultInput(quint32 id) override { inputDefaultRequests.append(id); return actionResult; }
    bool setInputVolume(quint32 id, double gain) override { inputGainNode = id; inputGain = gain; return actionResult; }
    bool setInputMute(quint32 id, bool muted) override { inputMuteNode = id; inputMute = muted; return actionResult; }

    void publishInputs(const QVector<Astrea::System::AudioOutput> &inputs, quint32 defaultId)
    {
        if (m_callbacks.inputsChanged) m_callbacks.inputsChanged(inputs, defaultId);
    }
    void publishInputLevel(double linear, bool muted)
    {
        if (m_callbacks.inputVolumeChanged) m_callbacks.inputVolumeChanged(linear, muted);
    }

    void publishOutputs(const QVector<Astrea::System::AudioOutput> &outputs,
                        quint32 defaultNodeId)
    {
        if (m_callbacks.outputsChanged)
            m_callbacks.outputsChanged(outputs, defaultNodeId);
    }

    void publishVolume(double linear, bool muted)
    {
        if (m_callbacks.volumeChanged)
            m_callbacks.volumeChanged(linear, muted);
    }

    void publishError(const QString &error)
    {
        if (m_callbacks.errorChanged)
            m_callbacks.errorChanged(error);
    }

    Callbacks currentCallbacks() const { return m_callbacks; }

    bool startResult = true;
    bool actionResult = true;
    QString startupError = QStringLiteral("PipeWire is unavailable");
    int startCount = 0;
    int stopCount = 0;
    int setVolumeCount = 0;
    int setMuteCount = 0;
    QList<quint32> defaultOutputRequests;
    QList<quint32> inputDefaultRequests;
    quint32 inputGainNode = 0;
    double inputGain = 0;
    quint32 inputMuteNode = 0;
    bool inputMute = false;
    quint32 volumeNodeId = 0;
    double volumeLinear = 0.0;
    quint32 muteNodeId = 0;
    bool muteValue = false;

private:
    Callbacks m_callbacks;
};

Astrea::System::AudioOutput output(quint32 nodeId, const QString &name,
                                   const QString &description, const QString &nick,
                                   bool isVirtual = false)
{
    return {nodeId, name, description, nick, QStringLiteral("Audio/Sink"), false, isVirtual};
}

class AudioPageFixture final {
public:
    explicit AudioPageFixture(bool nullAudio = false, bool backendStarts = true)
    {
        m_environmentGuard = std::make_unique<XdgConfigHome>(m_configHome.path());
        auto backend = std::make_unique<PageAudioBackend>();
        m_backend = backend.get();
        m_backend->startResult = backendStarts;
        auto service = std::make_unique<Astrea::System::AudioService>(std::move(backend));
        m_controller = std::make_unique<SettingsController>(
            std::make_unique<SettingsNavigationModel>(), SettingsUserProfile{},
            SettingsIconResolver{}, nullptr, std::move(service));
        m_controller->navigateTo(QStringLiteral("audio"));
        if (!nullAudio)
            m_controller->audio()->start();

        m_engine.addImportPath(QStringLiteral("qrc:/"));
        m_engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"),
                                                   nullAudio
                                                       ? static_cast<QObject *>(&m_nullAudioController)
                                                       : static_cast<QObject *>(m_controller.get()));
        m_engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &m_i18n);
        m_engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &m_theme);
        m_component = std::make_unique<QQmlComponent>(
            &m_engine, m_controller->selectedPageSource());
        m_window.resize(1000, 760);
    }

    ~AudioPageFixture() { destroyPage(); }

    QQuickItem *createPage()
    {
        QObject *object = m_component->create();
        if (!object)
            return nullptr;
        m_page = qobject_cast<QQuickItem *>(object);
        if (!m_page) {
            delete object;
            return nullptr;
        }
        m_page->setParentItem(m_window.contentItem());
        m_page->setSize(m_window.size());
        m_window.show();
        QCoreApplication::processEvents();
        return m_page;
    }

    void publishOutputs(const QVector<Astrea::System::AudioOutput> &outputs,
                        quint32 defaultNodeId)
    {
        m_backend->publishOutputs(outputs, defaultNodeId);
        QCoreApplication::processEvents();
    }

    void publishInputs(const QVector<Astrea::System::AudioOutput> &inputs, quint32 defaultId)
    {
        m_backend->publishInputs(inputs, defaultId);
        QCoreApplication::processEvents();
    }
    void publishInputLevel(double linear, bool muted)
    {
        m_backend->publishInputLevel(linear, muted);
        QCoreApplication::processEvents();
    }
    void publishVolume(double linear, bool muted)
    {
        m_backend->publishVolume(linear, muted);
        QCoreApplication::processEvents();
    }

    QString componentErrors() const { return m_component->errorString(); }
    QString configHomePath() const { return m_configHome.path(); }
    PageAudioBackend *backend() const { return m_backend; }
    SettingsController *controller() const { return m_controller.get(); }
    QQuickItem *page() const { return m_page; }
    QQuickWindow *window() { return &m_window; }

    QObject *findObject(const QString &objectName) const
    {
        if (!m_page)
            return nullptr;
        if (QObject *object = m_page->findChild<QObject *>(objectName))
            return object;
        return findVisualObject(m_page, objectName);
    }

    void destroyPage()
    {
        if (!m_page)
            return;
        m_page->setParentItem(nullptr);
        delete m_page;
        m_page = nullptr;
    }

private:
    static QObject *findVisualObject(QQuickItem *item, const QString &objectName)
    {
        if (!item)
            return nullptr;
        if (item->objectName() == objectName)
            return item;
        for (QQuickItem *child : item->childItems()) {
            if (QObject *match = findVisualObject(child, objectName))
                return match;
        }
        return nullptr;
    }

    QTemporaryDir m_configHome;
    std::unique_ptr<XdgConfigHome> m_environmentGuard;
    FakeI18n m_i18n;
    ThemeController m_theme;
    NullAudioController m_nullAudioController;
    std::unique_ptr<SettingsController> m_controller;
    QQmlApplicationEngine m_engine;
    std::unique_ptr<QQmlComponent> m_component;
    QQuickWindow m_window;
    QQuickItem *m_page = nullptr;
    PageAudioBackend *m_backend = nullptr;
};

} // namespace

class SettingsAudioQmlTest final : public QObject {
    Q_OBJECT

private slots:
    void pageLoadsFromAudioRouteAndShowsHumanOutputNames();
    void inputDevicesAndControlsFollowBackend();
    void headsetOutputUsesCenteredHeadsetIcon();
    void defaultOutputSelectionWaitsForAuthoritativeModelState();
    void volumeAndMuteUseSharedAudioServiceState();
    void appleVolumeEndpointsUseLocalSfSymbols();
    void incompleteDefaultStateKeepsOutputsVisibleAndControlsUnavailable();
    void emptyOutputStateAndServiceHealthRemainUsable();
    void virtualOutputRemainsVisibleAndSelectable();
    void nullAudioServiceHasStableUnavailablePresentation();
    void spatialAudioToggleUsesPersistedStateWhenAudioServiceIsUnavailable();
};

void SettingsAudioQmlTest::pageLoadsFromAudioRouteAndShowsHumanOutputNames()
{
    AudioPageFixture fixture;
    QCOMPARE(fixture.controller()->currentDestinationId(), QStringLiteral("audio"));
    QCOMPARE(fixture.controller()->selectedPageSource(),
             QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Audio.qml")));

    QQuickItem *page = fixture.createPage();
    QVERIFY2(page != nullptr, qPrintable(fixture.componentErrors()));
    QCOMPARE(page->objectName(), QStringLiteral("audioPage"));
    QCOMPARE(fixture.findObject(QStringLiteral("audioPageTitle"))->property("text").toString(),
             QStringLiteral("Audio"));

    fixture.publishOutputs({
        output(31, QStringLiteral("alsa_output.pci-0000_01_00.1.hdmi-stereo"),
               QStringLiteral("LG TV"), QStringLiteral("HDMI")),
    }, 31);
    QObject *outputRow = fixture.findObject(QStringLiteral("audioOutputRow_31"));
    QVERIFY2(outputRow != nullptr, qPrintable(fixture.componentErrors()));
    QCOMPARE(outputRow->property("implicitHeight").toInt(), 56);
    QCOMPARE(outputRow->property("deviceGlyph").toString(), QStringLiteral("\uf108"));
    QVERIFY(!fixture.findObject(QStringLiteral("audioNoOutputs"))->property("visible").toBool());
    QCOMPARE(fixture.findObject(QStringLiteral("audioOutputName_31"))->property("text").toString(),
             QStringLiteral("LG TV"));
    QCOMPARE(fixture.findObject(QStringLiteral("audioOutputDefault_31"))->property("visible").toBool(),
             true);
    QVERIFY(!fixture.findObject(QStringLiteral("audioVolumeSlider"))->property("enabled").toBool());
}

void SettingsAudioQmlTest::inputDevicesAndControlsFollowBackend()
{
    AudioPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));
    auto microphone = output(91, QStringLiteral("alsa_input.usb-microphone"),
                             QStringLiteral("USB Microphone"), QStringLiteral("Mic"));
    microphone.mediaClass = QStringLiteral("Audio/Source");
    fixture.publishInputs({microphone}, 91);
    QObject *row = fixture.findObject(QStringLiteral("audioInputRow_91"));
    QVERIFY(row != nullptr);
    QCOMPARE(row->property("deviceGlyph").toString(), QStringLiteral("\uf130"));
    QVERIFY(!fixture.findObject(QStringLiteral("audioNoInputs"))->property("visible").toBool());
    fixture.publishInputLevel(Astrea::System::AudioService::uiPercentToLinear(55), false);
    QCOMPARE(fixture.findObject(QStringLiteral("audioInputVolumePercent"))->property("text").toString(), QStringLiteral("55%"));
    QObject *slider = fixture.findObject(QStringLiteral("audioInputVolumeSlider"));
    QVERIFY(slider->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(slider, "valueEdited", Q_ARG(qreal, 65.0)));
    QCOMPARE(fixture.backend()->inputGainNode, quint32(91));
    QCOMPARE(fixture.backend()->inputGain, Astrea::System::AudioService::uiPercentToLinear(65));
    QObject *toggle = fixture.findObject(QStringLiteral("audioInputMuteToggle"));
    QVERIFY(QMetaObject::invokeMethod(toggle, "toggled", Q_ARG(bool, true)));
    QCOMPARE(fixture.backend()->inputMuteNode, quint32(91));
    QVERIFY(fixture.backend()->inputMute);
}

void SettingsAudioQmlTest::headsetOutputUsesCenteredHeadsetIcon()
{
    AudioPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));
    fixture.publishOutputs({
        output(83, QStringLiteral("alsa_output.usb-Corsair_HS60_PRO.analog-stereo"),
               QStringLiteral("Corsair HS60 PRO Surround USB Sound Adapter"),
               QStringLiteral("Corsair HS60 PRO")),
    }, 83);
    QObject *row = fixture.findObject(QStringLiteral("audioOutputRow_83"));
    QObject *selector = fixture.findObject(QStringLiteral("audioOutputSelector_83"));
    QVERIFY(row != nullptr);
    QVERIFY(selector != nullptr);
    QCOMPARE(row->property("deviceGlyph").toString(), QStringLiteral("\uee59"));
    QCOMPARE(selector->property("padding").toInt(), 0);
    QVERIFY(!selector->property("enabled").toBool()); // Already the default output
}

void SettingsAudioQmlTest::defaultOutputSelectionWaitsForAuthoritativeModelState()
{
    AudioPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));
    const auto speaker = output(41, QStringLiteral("alsa_output.usb-speakers"),
                                QStringLiteral("Desk Speakers"), QStringLiteral("Speakers"));
    const auto display = output(42, QStringLiteral("alsa_output.hdmi"),
                                QStringLiteral("Display Audio"), QStringLiteral("HDMI"));
    fixture.publishOutputs({speaker, display}, 41);

    QObject *displayRow = fixture.findObject(QStringLiteral("audioOutputRow_42"));
    QVERIFY(displayRow != nullptr);
    QCOMPARE(fixture.findObject(QStringLiteral("audioOutputDefault_41"))->property("visible").toBool(),
             true);
    QVERIFY(!fixture.findObject(QStringLiteral("audioOutputDefault_42"))->property("visible").toBool());
    QQuickItem *displayRowItem = qobject_cast<QQuickItem *>(displayRow);
    QQuickItem *displaySelector = qobject_cast<QQuickItem *>(
        fixture.findObject(QStringLiteral("audioOutputSelector_42")));
    QVERIFY(displayRowItem != nullptr);
    QVERIFY(displaySelector != nullptr);
    // Clicking the label/row must not switch outputs; only the circular button does.
    const QPoint labelPosition = displayRowItem->mapToScene(
        QPointF(displayRowItem->width() / 2, displayRowItem->height() / 2)).toPoint();
    QTest::mouseClick(fixture.window(), Qt::LeftButton, Qt::NoModifier, labelPosition);
    QVERIFY(fixture.backend()->defaultOutputRequests.isEmpty());
    displaySelector->forceActiveFocus();
    fixture.backend()->actionResult = false;
    QTest::keyClick(fixture.window(), Qt::Key_Space);
    QCoreApplication::processEvents();
    QCOMPARE(fixture.backend()->defaultOutputRequests, QList<quint32>{42});

    // A successful request does not make the requester's choice authoritative.
    QVERIFY(fixture.findObject(QStringLiteral("audioOutputDefault_41"))->property("visible").toBool());
    QVERIFY(!fixture.findObject(QStringLiteral("audioOutputDefault_42"))->property("visible").toBool());

    fixture.backend()->actionResult = true;
    const QPoint selectorPosition = displaySelector->mapToScene(
        QPointF(displaySelector->width() / 2, displaySelector->height() / 2)).toPoint();
    QTest::mouseClick(fixture.window(), Qt::LeftButton, Qt::NoModifier, selectorPosition);
    const QList<quint32> expectedRequests{42, 42};
    QCOMPARE(fixture.backend()->defaultOutputRequests, expectedRequests);
    QVERIFY(fixture.findObject(QStringLiteral("audioOutputDefault_41"))->property("visible").toBool());
    QVERIFY(!fixture.findObject(QStringLiteral("audioOutputDefault_42"))->property("visible").toBool());

    fixture.publishOutputs({speaker, display}, 42);
    QObject *newDefaultSelector = fixture.findObject(QStringLiteral("audioOutputSelector_42"));
    QVERIFY(newDefaultSelector != nullptr);
    QVERIFY(!newDefaultSelector->property("enabled").toBool());
    QVERIFY(fixture.findObject(QStringLiteral("audioOutputDefault_42"))->property("visible").toBool());
    QVERIFY(!fixture.findObject(QStringLiteral("audioOutputDefault_41"))->property("visible").toBool());
}

void SettingsAudioQmlTest::volumeAndMuteUseSharedAudioServiceState()
{
    AudioPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));
    fixture.publishOutputs({output(51, QStringLiteral("alsa_output.usb-dac"),
                                  QStringLiteral("USB DAC"), QStringLiteral("DAC"))}, 51);
    fixture.publishVolume(Astrea::System::AudioService::uiPercentToLinear(50.0), false);

    QObject *slider = fixture.findObject(QStringLiteral("audioVolumeSlider"));
    QVERIFY(slider != nullptr);
    QCOMPARE(slider->property("value").toDouble(), 50.0);
    QVERIFY(!slider->property("showEndpointGlyphs").toBool());
    QVERIFY(!slider->property("showValueTooltip").toBool());
    QVERIFY(!slider->property("tooltipVisible").toBool());
    QCOMPARE(fixture.findObject(QStringLiteral("audioVolumeLowIcon"))->property("text").toString(),
             QStringLiteral("\uf027"));
    QCOMPARE(fixture.findObject(QStringLiteral("audioVolumeHighIcon"))->property("text").toString(),
             QStringLiteral("\uf028"));
    QCOMPARE(fixture.findObject(QStringLiteral("audioVolumePercent"))->property("text").toString(),
             QStringLiteral("50%"));
    QVERIFY(!fixture.findObject(QStringLiteral("audioServiceStatus"))->property("visible").toBool());
    QVERIFY(slider->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(slider, "valueEdited", Q_ARG(qreal, 75.0)));
    QCOMPARE(fixture.backend()->setVolumeCount, 1);
    QCOMPARE(fixture.backend()->volumeNodeId, quint32(51));
    QCOMPARE(fixture.backend()->volumeLinear,
             Astrea::System::AudioService::uiPercentToLinear(75.0));
    QCOMPARE(fixture.controller()->audio()->volume(), 75.0);

    fixture.publishVolume(Astrea::System::AudioService::uiPercentToLinear(64.0), false);
    QCOMPARE(slider->property("value").toDouble(), 64.0);

    QObject *mute = fixture.findObject(QStringLiteral("audioMuteToggle"));
    QVERIFY(mute != nullptr);
    QVERIFY(mute->property("enabled").toBool());
    QVERIFY(!mute->property("checked").toBool());
    QVERIFY(QMetaObject::invokeMethod(mute, "toggled", Q_ARG(bool, true)));
    QCOMPARE(fixture.backend()->setMuteCount, 1);
    QCOMPARE(fixture.backend()->muteNodeId, quint32(51));
    QVERIFY(fixture.backend()->muteValue);
    QVERIFY(fixture.controller()->audio()->muted());
    QVERIFY(mute->property("checked").toBool());
}

void SettingsAudioQmlTest::appleVolumeEndpointsUseLocalSfSymbols()
{
    QTemporaryDir dataHome;
    QVERIFY(dataHome.isValid());
    const QByteArray oldData = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());
    const auto restoreData = qScopeGuard([oldData] {
        if (oldData.isNull()) qunsetenv("XDG_DATA_HOME");
        else qputenv("XDG_DATA_HOME", oldData);
    });

    const QString directory = dataHome.path() + QStringLiteral("/AstreaOS/Themes/AppleOS");
    QVERIFY(QDir().mkpath(directory));
    const QByteArray svg = "<svg xmlns='http://www.w3.org/2000/svg' "
                           "viewBox='0 0 24 24'><path fill='black' "
                           "d='M2 2h20v20H2z'/></svg>";
    for (const QString &name : {QStringLiteral("volume-min"),
                                QStringLiteral("volume-max")}) {
        QFile file(directory + QLatin1Char('/') + name + QStringLiteral(".svg"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(svg), svg.size());
    }

    AudioPageFixture fixture;
    QVERIFY(fixture.controller()->setNavigationIconTheme(QStringLiteral("apple")));
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));
    fixture.publishOutputs({output(52, QStringLiteral("alsa_output.test"),
                                  QStringLiteral("Test speaker"), QStringLiteral("Test"))}, 52);
    fixture.publishVolume(Astrea::System::AudioService::uiPercentToLinear(45.0), false);

    auto *min = fixture.findObject(QStringLiteral("audioVolumeMinAppleIcon"));
    auto *max = fixture.findObject(QStringLiteral("audioVolumeMaxAppleIcon"));
    QVERIFY(min != nullptr);
    QVERIFY(max != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(min->property("visible").toBool(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(max->property("visible").toBool(), 2000);
    QCOMPARE(min->property("source").toUrl(),
             QUrl::fromLocalFile(directory + QStringLiteral("/volume-min.svg")));
    QCOMPARE(max->property("source").toUrl(),
             QUrl::fromLocalFile(directory + QStringLiteral("/volume-max.svg")));
    QVERIFY(!fixture.findObject(QStringLiteral("audioVolumeLowIcon"))
                ->property("visible").toBool());
    QVERIFY(!fixture.findObject(QStringLiteral("audioVolumeHighIcon"))
                ->property("visible").toBool());

    QVERIFY(fixture.controller()->setNavigationIconTheme(QStringLiteral("lucide")));
    QTRY_VERIFY_WITH_TIMEOUT(!min->property("visible").toBool(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!max->property("visible").toBool(), 1000);
    QVERIFY(fixture.findObject(QStringLiteral("audioVolumeLowIcon"))
                ->property("visible").toBool());
    QVERIFY(fixture.findObject(QStringLiteral("audioVolumeHighIcon"))
                ->property("visible").toBool());
}

void SettingsAudioQmlTest::incompleteDefaultStateKeepsOutputsVisibleAndControlsUnavailable()
{
    AudioPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));
    fixture.publishOutputs({output(61, QStringLiteral("alsa_output.monitor"),
                                  QStringLiteral("Monitor Audio"), QString())}, 61);

    QVERIFY(fixture.findObject(QStringLiteral("audioOutputRow_61")) != nullptr);
    QObject *slider = fixture.findObject(QStringLiteral("audioVolumeSlider"));
    QObject *mute = fixture.findObject(QStringLiteral("audioMuteToggle"));
    QVERIFY(slider != nullptr);
    QVERIFY(mute != nullptr);
    QVERIFY(!slider->property("enabled").toBool());
    QVERIFY(!mute->property("enabled").toBool());
    QVERIFY(fixture.findObject(QStringLiteral("audioVolumeLoading"))->property("visible").toBool());
    QVERIFY(fixture.findObject(QStringLiteral("audioMuteLoading"))->property("visible").toBool());

    fixture.publishVolume(Astrea::System::AudioService::uiPercentToLinear(42.0), true);
    QVERIFY(slider->property("enabled").toBool());
    QVERIFY(mute->property("enabled").toBool());
    QVERIFY(mute->property("checked").toBool());
}

void SettingsAudioQmlTest::emptyOutputStateAndServiceHealthRemainUsable()
{
    AudioPageFixture emptyFixture;
    QVERIFY2(emptyFixture.createPage() != nullptr, qPrintable(emptyFixture.componentErrors()));
    QVERIFY(emptyFixture.findObject(QStringLiteral("audioNoOutputs"))->property("visible").toBool());

    AudioPageFixture unavailableFixture(false, false);
    QVERIFY2(unavailableFixture.createPage() != nullptr, qPrintable(unavailableFixture.componentErrors()));
    QCOMPARE(unavailableFixture.findObject(QStringLiteral("audioServiceStatus"))->property("text").toString(),
             QStringLiteral("Audio service is unavailable"));
    QCOMPARE(unavailableFixture.findObject(QStringLiteral("audioServiceError"))->property("text").toString(),
             QStringLiteral("PipeWire is unavailable"));
    QVERIFY(!unavailableFixture.findObject(QStringLiteral("audioVolumeSlider"))
                 ->property("enabled").toBool());

    AudioPageFixture degradedFixture;
    QVERIFY2(degradedFixture.createPage() != nullptr, qPrintable(degradedFixture.componentErrors()));
    degradedFixture.backend()->publishError(QStringLiteral("PipeWire connection lost"));
    QCoreApplication::processEvents();
    QCOMPARE(degradedFixture.findObject(QStringLiteral("audioServiceStatus"))->property("text").toString(),
             QStringLiteral("Reconnecting to audio…"));
    QCOMPARE(degradedFixture.findObject(QStringLiteral("audioServiceError"))->property("text").toString(),
             QStringLiteral("PipeWire connection lost"));
    QVERIFY(!degradedFixture.findObject(QStringLiteral("audioVolumeSlider"))
                 ->property("enabled").toBool());

    degradedFixture.backend()->currentCallbacks().defaultStateChanged(true, true, {});
    QCoreApplication::processEvents();
    QCOMPARE(degradedFixture.findObject(QStringLiteral("audioServiceStatus"))->property("text").toString(),
             QStringLiteral("Waiting for current audio state…"));
}

void SettingsAudioQmlTest::virtualOutputRemainsVisibleAndSelectable()
{
    AudioPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));
    fixture.publishOutputs({output(71, QStringLiteral("astrea_spatial_output"),
                                  QStringLiteral("Astrea Spatial Audio"), QString(), true)}, 0);

    QObject *selector = fixture.findObject(QStringLiteral("audioOutputSelector_71"));
    QVERIFY(selector != nullptr);
    QCOMPARE(fixture.findObject(QStringLiteral("audioOutputName_71"))->property("text").toString(),
             QStringLiteral("Astrea Spatial Audio"));
    QCOMPARE(fixture.findObject(QStringLiteral("audioOutputDetail_71"))->property("text").toString(),
             QStringLiteral("Virtual audio output"));
    QVERIFY(QMetaObject::invokeMethod(selector, "clicked"));
    QCOMPARE(fixture.backend()->defaultOutputRequests, QList<quint32>{71});
}

void SettingsAudioQmlTest::nullAudioServiceHasStableUnavailablePresentation()
{
    AudioPageFixture fixture(true);
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));
    QCOMPARE(fixture.findObject(QStringLiteral("audioServiceStatus"))->property("text").toString(),
             QStringLiteral("Audio service is unavailable"));
    QVERIFY(!fixture.findObject(QStringLiteral("audioVolumeSlider"))->property("enabled").toBool());
    QVERIFY(!fixture.findObject(QStringLiteral("audioMuteToggle"))->property("enabled").toBool());
    QVERIFY(fixture.findObject(QStringLiteral("audioOutputsUnavailable"))->property("visible").toBool());
}

void SettingsAudioQmlTest::spatialAudioToggleUsesPersistedStateWhenAudioServiceIsUnavailable()
{
    AudioPageFixture fixture(false, false);
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));
    QVERIFY(!fixture.controller()->audio()->available());

    QObject *toggle = fixture.findObject(QStringLiteral("spatialAudioToggle"));
    QVERIFY(toggle != nullptr);
    QVERIFY(toggle->property("enabled").toBool());
    QVERIFY(!toggle->property("checked").toBool());

    QVERIFY(QMetaObject::invokeMethod(toggle, "toggled", Q_ARG(bool, true)));
    QTRY_VERIFY_WITH_TIMEOUT(fixture.controller()->spatialAudio()->spatialAudioEnabled(), 1000);
    QVERIFY(toggle->property("checked").toBool());
    QVERIFY(fixture.controller()->spatialAudio()->spatialAudioConfigValid());
    QVERIFY(fixture.controller()->spatialAudio()->spatialAudioConfigError().isEmpty());
    QCOMPARE(fixture.backend()->defaultOutputRequests.size(), 0);

    const QString path = spatialAudioConfigPath(fixture.configHomePath());
    QFile saved(path);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QCOMPARE(QJsonDocument::fromJson(saved.readAll()).object()
                 .value(QStringLiteral("enabled")).toBool(), true);

    QVERIFY(writeConfigAtomically(path, QByteArrayLiteral(R"({"enabled":false})")));
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.controller()->spatialAudio()->spatialAudioEnabled(), 3000);
    QVERIFY(!toggle->property("checked").toBool());
    QCOMPARE(fixture.backend()->defaultOutputRequests.size(), 0);

    const QString audioDirectory = QFileInfo(path).absolutePath();
    QVERIFY(QDir().rename(audioDirectory, audioDirectory + QStringLiteral(".saved")));
    QFile obstruction(audioDirectory);
    QVERIFY(obstruction.open(QIODevice::WriteOnly));
    QCOMPARE(obstruction.write(QByteArrayLiteral("blocked")), qsizetype(7));
    obstruction.close();

    QVERIFY(QMetaObject::invokeMethod(toggle, "toggled", Q_ARG(bool, true)));
    QCoreApplication::processEvents();
    QVERIFY(!toggle->property("checked").toBool());
    QObject *diagnostic = fixture.findObject(QStringLiteral("spatialAudioConfigError"));
    QVERIFY(diagnostic != nullptr);
    QVERIFY(diagnostic->property("visible").toBool());
    QVERIFY(!diagnostic->property("text").toString().isEmpty());

    AudioPageFixture stoppedFixture;
    stoppedFixture.controller()->audio()->stop();
    QVERIFY(!stoppedFixture.controller()->audio()->available());
    QVERIFY(!stoppedFixture.controller()->audio()->ready());
    QVERIFY2(stoppedFixture.createPage() != nullptr,
             qPrintable(stoppedFixture.componentErrors()));
    QCOMPARE(stoppedFixture.findObject(QStringLiteral("audioServiceStatus"))
                 ->property("text").toString(),
             QStringLiteral("Audio service is inactive"));
    QObject *stoppedToggle =
        stoppedFixture.findObject(QStringLiteral("spatialAudioToggle"));
    QVERIFY(stoppedToggle != nullptr);
    QVERIFY(stoppedToggle->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(stoppedToggle, "toggled", Q_ARG(bool, true)));
    QVERIFY(stoppedToggle->property("checked").toBool());
}

QTEST_MAIN(SettingsAudioQmlTest)
#include "SettingsAudioQmlTest.moc"
