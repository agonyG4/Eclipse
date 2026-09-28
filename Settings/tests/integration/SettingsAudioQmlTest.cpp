#include "core/SettingsController.hpp"
#include "system/SystemServiceState.hpp"
#include "system/audio/AudioBackend.hpp"
#include "system/audio/AudioOutputModel.hpp"
#include "system/audio/AudioService.hpp"
#include "theme/ThemeController.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
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

public:
    QObject *audio() const { return nullptr; }
};

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

    void publishVolume(double linear, bool muted)
    {
        m_backend->publishVolume(linear, muted);
        QCoreApplication::processEvents();
    }

    QString componentErrors() const { return m_component->errorString(); }
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
    void defaultOutputSelectionWaitsForAuthoritativeModelState();
    void volumeAndMuteUseSharedAudioServiceState();
    void incompleteDefaultStateKeepsOutputsVisibleAndControlsUnavailable();
    void emptyOutputStateAndServiceHealthRemainUsable();
    void virtualOutputRemainsVisibleAndSelectable();
    void nullAudioServiceHasStableUnavailablePresentation();
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

    fixture.publishOutputs({
        output(31, QStringLiteral("alsa_output.pci-0000_01_00.1.hdmi-stereo"),
               QStringLiteral("LG TV"), QStringLiteral("HDMI")),
    }, 31);
    QVERIFY2(fixture.findObject(QStringLiteral("audioOutputRow_31")) != nullptr,
             qPrintable(fixture.componentErrors()));
    QCOMPARE(fixture.findObject(QStringLiteral("audioOutputName_31"))->property("text").toString(),
             QStringLiteral("LG TV"));
    QCOMPARE(fixture.findObject(QStringLiteral("audioOutputDefault_31"))->property("visible").toBool(),
             true);
    QVERIFY(!fixture.findObject(QStringLiteral("audioVolumeSlider"))->property("enabled").toBool());
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
    QVERIFY(displayRowItem != nullptr);
    displayRowItem->forceActiveFocus();
    fixture.backend()->actionResult = false;
    QTest::keyClick(fixture.window(), Qt::Key_Space);
    QCoreApplication::processEvents();
    QCOMPARE(fixture.backend()->defaultOutputRequests, QList<quint32>{42});

    // A successful request does not make the requester's choice authoritative.
    QVERIFY(fixture.findObject(QStringLiteral("audioOutputDefault_41"))->property("visible").toBool());
    QVERIFY(!fixture.findObject(QStringLiteral("audioOutputDefault_42"))->property("visible").toBool());

    fixture.backend()->actionResult = true;
    QVERIFY(QMetaObject::invokeMethod(displayRow, "clicked"));
    const QList<quint32> expectedRequests{42, 42};
    QCOMPARE(fixture.backend()->defaultOutputRequests, expectedRequests);
    QVERIFY(fixture.findObject(QStringLiteral("audioOutputDefault_41"))->property("visible").toBool());
    QVERIFY(!fixture.findObject(QStringLiteral("audioOutputDefault_42"))->property("visible").toBool());

    fixture.publishOutputs({speaker, display}, 42);
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

    QObject *row = fixture.findObject(QStringLiteral("audioOutputRow_71"));
    QVERIFY(row != nullptr);
    QCOMPARE(fixture.findObject(QStringLiteral("audioOutputName_71"))->property("text").toString(),
             QStringLiteral("Astrea Spatial Audio"));
    QCOMPARE(fixture.findObject(QStringLiteral("audioOutputDetail_71"))->property("text").toString(),
             QStringLiteral("Virtual audio output"));
    QVERIFY(QMetaObject::invokeMethod(row, "clicked"));
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

QTEST_MAIN(SettingsAudioQmlTest)
#include "SettingsAudioQmlTest.moc"
