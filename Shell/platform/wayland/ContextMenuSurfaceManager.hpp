#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>

#include <functional>

namespace Astrea::Shell {
class ContextMenuController;
}
class ContextMenuSurfaceBundle;
class QGuiApplication;
class QQmlApplicationEngine;
class QScreen;
namespace Astrea::StatusNotifier {
class StatusNotifierService;
}

class ContextMenuSurfaceManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int bundleCount READ bundleCount NOTIFY bundleCountChanged)
    Q_PROPERTY(bool overlayMapped READ overlayMapped NOTIFY mappingChanged)
    Q_PROPERTY(bool layerConfigurationRequested READ layerConfigurationRequested NOTIFY layerStateChanged)

public:
    using BundleFactory = std::function<ContextMenuSurfaceBundle *(QScreen *, QObject *)>;

    ContextMenuSurfaceManager(QGuiApplication &application, QQmlApplicationEngine &engine,
                              Astrea::Shell::ContextMenuController *controller,
                              QObject *parent = nullptr,
                              BundleFactory bundleFactory = {},
                              Astrea::StatusNotifier::StatusNotifierService *statusNotifier = nullptr);
    ~ContextMenuSurfaceManager() override;

    bool initialize(QString *errorOut = nullptr);
    void shutdown();
    int bundleCount() const { return m_bundles.size(); }
    bool overlayMapped() const;
    bool layerConfigurationRequested() const;

signals:
    void bundleCountChanged();
    void mappingChanged();
    void layerStateChanged();

private:
    bool addScreen(QScreen *screen, QString *errorOut = nullptr);
    void removeScreen(QScreen *screen);
    void handleScreenGeometryChanged(QScreen *screen);
    void syncMappingNotification();

    QGuiApplication &m_application;
    QQmlApplicationEngine &m_engine;
    Astrea::Shell::ContextMenuController *m_controller = nullptr;
    Astrea::StatusNotifier::StatusNotifierService *m_statusNotifier = nullptr;
    QHash<QScreen *, ContextMenuSurfaceBundle *> m_bundles;
    BundleFactory m_bundleFactory;
    bool m_initialized = false;
    bool m_stopped = false;
    bool m_lastOverlayMapped = false;
};
