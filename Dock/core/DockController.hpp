#pragma once

#include "core/DockAppModel.hpp"
#include "services/DockConfigWatcher.hpp"
#include "apps/DesktopEntryCatalog.hpp"
#include "launch/ApplicationLauncher.hpp"

#include <QHash>
#include <QObject>
#include <QSet>

#include <optional>

class TyphonToplevelConnection;

class DockController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(DockAppModel *appModel READ appModel CONSTANT)
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(int pinCount READ pinCount NOTIFY modelChanged)
    Q_PROPERTY(int resolvedPinCount READ resolvedPinCount NOTIFY modelChanged)
    Q_PROPERTY(int launchingCount READ launchingCount NOTIFY modelChanged)
    Q_PROPERTY(bool runtimeKnown READ runtimeKnown NOTIFY modelChanged)
    Q_PROPERTY(int iconSize READ iconSize NOTIFY configChanged)
    Q_PROPERTY(int bottomMargin READ bottomMargin NOTIFY configChanged)
    Q_PROPERTY(int panelPadding READ panelPadding NOTIFY configChanged)
    Q_PROPERTY(int itemSpacing READ itemSpacing NOTIFY configChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit DockController(ApplicationLauncher *launcher = nullptr,
                            DesktopEntryCatalog *catalog = nullptr,
                            QObject *parent = nullptr);

    DockAppModel *appModel() { return &m_model; }
    bool enabled() const { return m_enabled; }
    bool visible() const;
    int pinCount() const { return m_config.pins.size(); }
    int resolvedPinCount() const;
    int launchingCount() const;
    bool runtimeKnown() const { return m_runtimeKnown; }
    DesktopEntryCatalog *catalog() const { return m_catalog; }
    ApplicationLauncher *launcher() const { return m_launcher; }
    int iconSize() const { return m_config.iconSize; }
    int bottomMargin() const { return m_config.bottomMargin; }
    int panelPadding() const { return m_config.panelPadding; }
    int itemSpacing() const { return m_config.itemSpacing; }
    QString lastError() const { return m_lastError; }

    void applyConfig(const DockConfig &config);
    void setComponentEnabled(bool enabled);
    void setCatalogSnapshot(std::shared_ptr<const DesktopEntrySnapshot> snapshot);
    void attachTyphonConnection(TyphonToplevelConnection *connection);
    void applyTyphonSnapshot(const Astrea::Typhon::Snapshot &snapshot);
    void clearTyphonRuntime();

    Q_INVOKABLE void launch(int row);
    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();

signals:
    void enabledChanged();
    void visibleChanged();
    void modelChanged();
    void configChanged();
    void lastErrorChanged();

private slots:
    void onLaunchSucceeded(const QString &desktopId);
    void onLaunchFailed(const QString &desktopId, const QString &error);
    void onLaunchTimedOut(const QString &desktopId);
    void onTyphonActionFinished(quint64 token, Astrea::Typhon::ToplevelAction action,
                                Astrea::Typhon::ToplevelActionResult result);
    void onTyphonActionFailed(quint64 token, Astrea::Typhon::ToplevelAction action,
                              Astrea::Typhon::ToplevelActionError error);

private:
    QString keyForLaunchId(const QString &desktopId) const;
    void finishLaunch(const QString &desktopId, bool success, const QString &error = {});
    void setLastError(const QString &error);
    void updateVisibility();
    void projectRuntime();
    void reconcileTyphonActionFailure(Astrea::Typhon::ToplevelActionError error);

    DockAppModel m_model;
    DockConfig m_config;
    ApplicationLauncher *m_launcher = nullptr;
    DesktopEntryCatalog *m_catalog = nullptr;
    std::shared_ptr<const DesktopEntrySnapshot> m_catalogSnapshot;
    QHash<QString, QString> m_pendingLaunches;
    bool m_enabled = true;
    bool m_requestedVisible = true;
    bool m_visible = false;
    bool m_runtimeKnown = false;
    std::optional<Astrea::Typhon::Snapshot> m_runtimeSnapshot;
    QHash<QString, Astrea::Typhon::DockApplicationRuntimeState> m_runtimeStates;
    QHash<quint64, QString> m_pendingActivations;
    quint64 m_nextActivationToken = 0;
    TyphonToplevelConnection *m_typhonConnection = nullptr;
    QVector<QMetaObject::Connection> m_typhonConnections;
    Astrea::Typhon::DockApplicationStateProjector m_runtimeProjector;
    QString m_lastError;
};
