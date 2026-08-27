#include "core/ContextMenuController.hpp"
#include "core/ContextMenuModel.hpp"
#include "core/ContextMenuSurfaceMapping.hpp"
#include "platform/wayland/ContextMenuSurfaceBundle.hpp"
#include "platform/wayland/ContextMenuSurfaceManager.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>

namespace {

class RecordingContextMenuSurfaceBundle final : public ContextMenuSurfaceBundle {
public:
    RecordingContextMenuSurfaceBundle(QScreen *screen,
                                      Astrea::Shell::ContextMenuController *controller,
                                      QObject *parent)
        : ContextMenuSurfaceBundle(screen, nullptr, controller, parent)
        , m_controller(controller)
        , m_outputKey(ContextMenuSurfaceBundle::outputKey())
    {
        if (m_outputKey.isEmpty())
            m_outputKey = QStringLiteral("output-1");
        if (m_controller) {
            connect(m_controller, &Astrea::Shell::ContextMenuController::presentationChanged,
                    this, &RecordingContextMenuSurfaceBundle::syncMapping);
            connect(m_controller, &Astrea::Shell::ContextMenuController::lifecycleChanged,
                    this, &RecordingContextMenuSurfaceBundle::syncMapping);
        }
    }

    bool initialize(QString *) override
    {
        m_initialized = true;
        return true;
    }

    void map() override
    {
        m_mapped = true;
        syncMapping();
    }

    QString outputKey() const override { return m_outputKey; }
    bool overlayMapped() const override { return m_overlayMapped; }

    bool initialized() const { return m_initialized; }

private:
    void syncMapping()
    {
        const bool next = Astrea::Shell::ContextMenuSurfaceMapping::overlayShouldMap(
            outputKey(), m_mapped, m_controller && m_controller->hasActivePresentation(),
            m_controller ? m_controller->outputKey() : QString());
        if (next == m_overlayMapped)
            return;
        m_overlayMapped = next;
        emit mappingChanged();
    }

    Astrea::Shell::ContextMenuController *m_controller = nullptr;
    bool m_initialized = false;
    bool m_mapped = false;
    bool m_overlayMapped = false;
    QString m_outputKey;
};

} // namespace

class ContextMenuSurfaceManagerTest final : public QObject {
    Q_OBJECT

private slots:
    void notifiesEffectiveOverlayMappingTransitions();
};

void ContextMenuSurfaceManagerTest::notifiesEffectiveOverlayMappingTransitions()
{
    auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    QVERIFY(application);
    QScreen *screen = QGuiApplication::primaryScreen();
    QVERIFY(screen);

    Astrea::Shell::ContextMenuController controller;
    QQmlApplicationEngine engine;
    QList<RecordingContextMenuSurfaceBundle *> bundles;
    ContextMenuSurfaceManager manager(
        *application, engine, &controller, nullptr,
        [&bundles, &controller](QScreen *output, QObject *parent) {
            auto *bundle = new RecordingContextMenuSurfaceBundle(output, &controller, parent);
            bundles.append(bundle);
            return bundle;
        });
    QSignalSpy mappingSpy(&manager, &ContextMenuSurfaceManager::mappingChanged);

    QVERIFY(manager.initialize());
    QCOMPARE(bundles.size(), 1);
    QVERIFY(bundles.constFirst()->initialized());
    QVERIFY(!manager.overlayMapped());
    QCOMPARE(mappingSpy.count(), 0);

    const QString outputKey = bundles.constFirst()->outputKey();
    const auto target = Astrea::Shell::ContextMenuTarget{
        Astrea::Shell::ContextMenuTarget::Kind::Desktop, QStringLiteral("desktop"),
        outputKey};
    QVERIFY(controller.present(target,
                               {Astrea::Shell::ContextMenuModel::NodeSpec{
                                   .token = QStringLiteral("settings"),
                                   .label = QStringLiteral("Settings")}},
                               [](const QString &) { return true; }));
    QVERIFY(manager.overlayMapped());
    QCOMPARE(mappingSpy.count(), 1);

    QVERIFY(controller.present(target,
                               {Astrea::Shell::ContextMenuModel::NodeSpec{
                                   .token = QStringLiteral("other"),
                                   .label = QStringLiteral("Other")}},
                               [](const QString &) { return true; }));
    QVERIFY(manager.overlayMapped());
    QCOMPARE(mappingSpy.count(), 1);

    controller.close();
    QVERIFY(manager.overlayMapped());
    QCOMPARE(mappingSpy.count(), 1);
    controller.completeClose();
    QVERIFY(!manager.overlayMapped());
    QCOMPARE(mappingSpy.count(), 2);

    manager.shutdown();
    QCOMPARE(mappingSpy.count(), 2);
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication application(argc, argv);
    ContextMenuSurfaceManagerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ContextMenuSurfaceManagerTest.moc"
