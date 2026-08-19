#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QRect>

#include <functional>

class BarClockService;
class BarController;
class BarSurfaceBundle;
class QQmlApplicationEngine;
class QGuiApplication;
class QScreen;
class WorkspaceModel;

class BarSurfaceManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int bundleCount READ bundleCount NOTIFY bundleCountChanged)
    Q_PROPERTY(bool popupOpen READ popupOpen NOTIFY popupStateChanged)
    Q_PROPERTY(bool layerConfigurationRequested READ layerConfigurationRequested NOTIFY layerStateChanged)

public:
    using BundleFactory = std::function<BarSurfaceBundle *(QScreen *, QObject *)>;

    BarSurfaceManager(QGuiApplication &application, QQmlApplicationEngine &engine,
                      BarController *barController, BarClockService *clockService,
                      WorkspaceModel *workspaceModel, QObject *parent = nullptr,
                      BundleFactory bundleFactory = {});
    ~BarSurfaceManager() override;

    bool initialize(QString *errorOut = nullptr);
    void shutdown();

    int bundleCount() const { return m_bundles.size(); }
    bool popupOpen() const;
    bool layerConfigurationRequested() const;

signals:
    void bundleCountChanged();
    void popupStateChanged();
    void layerStateChanged();

private:
    bool addScreen(QScreen *screen, QString *errorOut = nullptr);
    void removeScreen(QScreen *screen);
    void handleScreenGeometryChanged(QScreen *screen);
    void syncBarEnablement();

    QGuiApplication &m_application;
    QQmlApplicationEngine &m_engine;
    BarController *m_barController = nullptr;
    BarClockService *m_clockService = nullptr;
    WorkspaceModel *m_workspaceModel = nullptr;
    QHash<QScreen *, BarSurfaceBundle *> m_bundles;
    BundleFactory m_bundleFactory;
    bool m_initialized = false;
};
