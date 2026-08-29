#pragma once

#include "core/DockAppModel.hpp"
#include "core/DockMetrics.hpp"
#include "core/DockSurfaceGeometry.hpp"
#include "services/DockConfigWatcher.hpp"
#include "services/DockConfigPersistence.hpp"
#include "apps/DesktopEntryCatalog.hpp"
#include "launch/ApplicationLauncher.hpp"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>

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
    Q_PROPERTY(int edgeMargin READ edgeMargin NOTIFY configChanged)
    Q_PROPERTY(int effectiveEdgeMargin READ effectiveEdgeMargin NOTIFY configChanged)
    Q_PROPERTY(int bottomMargin READ bottomMargin NOTIFY configChanged)
    Q_PROPERTY(QString position READ position NOTIFY configChanged)
    Q_PROPERTY(bool vertical READ vertical NOTIFY configChanged)
    Q_PROPERTY(bool floating READ floating NOTIFY configChanged)
    Q_PROPERTY(int cornerRadius READ cornerRadius NOTIFY configChanged)
    Q_PROPERTY(QString autoHide READ autoHide NOTIFY configChanged)
    Q_PROPERTY(QString indicatorStyle READ indicatorStyle NOTIFY configChanged)
    Q_PROPERTY(int indicatorSize READ indicatorSize NOTIFY configChanged)
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled NOTIFY configChanged)
    Q_PROPERTY(double animationSpeed READ animationSpeed NOTIFY configChanged)
    Q_PROPERTY(int chromeBottomMargin READ chromeBottomMargin CONSTANT)
    Q_PROPERTY(int iconRestingTop READ iconRestingTop NOTIFY configChanged)
    Q_PROPERTY(int panelPadding READ panelPadding NOTIFY configChanged)
    Q_PROPERTY(int itemSpacing READ itemSpacing NOTIFY configChanged)
    Q_PROPERTY(int delegateWidth READ delegateWidth NOTIFY configChanged)
    Q_PROPERTY(int delegateHeight READ delegateHeight NOTIFY configChanged)
    Q_PROPERTY(QString hoverEffect READ hoverEffect NOTIFY configChanged)
    // Kept as a derived compatibility property for older QML consumers.
    Q_PROPERTY(bool magnificationEnabled READ magnificationEnabled NOTIFY configChanged)
    Q_PROPERTY(double magnificationScale READ magnificationScale NOTIFY configChanged)
    Q_PROPERTY(double magnificationRadius READ magnificationRadius NOTIFY configChanged)
    Q_PROPERTY(int restingHeight READ restingHeight NOTIFY configChanged)
    Q_PROPERTY(int restingCrossThickness READ restingCrossThickness NOTIFY configChanged)
    Q_PROPERTY(int exclusiveZone READ exclusiveZone NOTIFY reservationChanged)
    Q_PROPERTY(int layerShellEdgeMargin READ layerShellEdgeMargin NOTIFY surfacePlacementChanged)
    Q_PROPERTY(int chromeEdgeInset READ chromeEdgeInset NOTIFY surfacePlacementChanged)
    Q_PROPERTY(bool physicalEdgeReveal READ physicalEdgeReveal NOTIFY surfacePlacementChanged)
    Q_PROPERTY(bool revealed READ revealed NOTIFY revealedChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit DockController(ApplicationLauncher *launcher = nullptr,
                            DesktopEntryCatalog *catalog = nullptr,
                            DockConfigPersistence *persistence = nullptr,
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
    int edgeMargin() const { return m_config.edgeMargin; }
    int effectiveEdgeMargin() const { return m_config.effectiveEdgeMargin(); }
    int bottomMargin() const { return edgeMargin(); }
    QString position() const { return m_config.position; }
    bool vertical() const { return m_config.position != QStringLiteral("bottom"); }
    bool floating() const { return m_config.floating; }
    int cornerRadius() const { return m_config.cornerRadius; }
    QString autoHide() const { return m_config.autoHide; }
    QString indicatorStyle() const { return m_config.indicatorStyle; }
    int indicatorSize() const { return m_config.indicatorSize; }
    bool animationsEnabled() const { return m_config.animationsEnabled; }
    double animationSpeed() const { return m_config.animationSpeed; }
    int chromeBottomMargin() const { return DockMetrics::chromeBottomMargin; }
    int iconRestingTop() const { return DockMetrics::iconRestingTop(m_config.iconSize); }
    int panelPadding() const { return m_config.panelPadding; }
    int itemSpacing() const { return m_config.itemSpacing; }
    int delegateWidth() const { return DockMetrics::delegateWidth(m_config.iconSize); }
    int delegateHeight() const { return DockMetrics::delegateHeight(m_config.iconSize); }
    QString hoverEffect() const { return m_config.hoverEffect; }
    bool magnificationEnabled() const
    { return m_config.hoverEffect == QStringLiteral("magnification"); }
    double magnificationScale() const { return m_config.magnificationScale; }
    double magnificationRadius() const { return m_config.magnificationRadius; }
    int restingHeight() const { return DockMetrics::restingHeight(m_config.iconSize); }
    int restingCrossThickness() const { return restingHeight(); }
    int exclusiveZone() const;
    DockSurfacePlacement surfacePlacement() const
    { return DockSurfaceGeometry::placementFor(m_config, autoHideActive()); }
    int layerShellEdgeMargin() const { return surfacePlacement().layerShellEdgeMargin; }
    int chromeEdgeInset() const { return surfacePlacement().chromeEdgeInset; }
    bool physicalEdgeReveal() const { return surfacePlacement().physicalEdgeReveal; }
    bool revealed() const { return m_revealed; }
    QString lastError() const { return m_lastError; }

    void applyConfig(const DockConfig &config);
    void setComponentEnabled(bool enabled);
    void setCatalogSnapshot(std::shared_ptr<const DesktopEntrySnapshot> snapshot);
    void attachTyphonConnection(TyphonToplevelConnection *connection);
    void applyTyphonSnapshot(const Astrea::Typhon::Snapshot &snapshot);
    void clearTyphonRuntime();
    Q_INVOKABLE void setPointerInside(bool inside);

    Q_INVOKABLE void launch(int row);
    Q_INVOKABLE void launchByDesktopFileName(const QString &desktopFileName);
    Q_INVOKABLE bool launchNewWindow(const QString &desktopFileName);
    QVector<Astrea::Typhon::Toplevel> windowsForDesktopFileName(
        const QString &desktopFileName) const;
    Q_INVOKABLE bool activateWindow(const QString &desktopFileName, const QString &windowId);
    Q_INVOKABLE bool closeWindow(const QString &desktopFileName, const QString &windowId);
    Q_INVOKABLE bool setPinned(const QString &desktopFileName, bool pinned);
    Q_INVOKABLE bool movePinned(const QString &desktopFileName, int targetPinIndex);
    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();

signals:
    void enabledChanged();
    void visibleChanged();
    void modelChanged();
    void configChanged();
    void reservationChanged();
    void surfacePlacementChanged();
    void revealedChanged();
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
    void updateAutoHidePolicy();
    bool autoHideActive() const;
    void setRevealed(bool revealed);
    void projectRuntime();
    void reconcileTyphonActionFailure(Astrea::Typhon::ToplevelActionError error);
    void launchItem(const DockAppInfo &item, bool activateRunning = true);
    bool requestExactWindowAction(const QString &desktopFileName, const QString &windowId,
                                  Astrea::Typhon::ToplevelAction action);

    DockAppModel m_model;
    DockConfig m_config;
    ApplicationLauncher *m_launcher = nullptr;
    DockConfigPersistence *m_persistence = nullptr;
    DesktopEntryCatalog *m_catalog = nullptr;
    std::shared_ptr<const DesktopEntrySnapshot> m_catalogSnapshot;
    QHash<QString, QString> m_pendingLaunches;
    bool m_enabled = true;
    bool m_requestedVisible = true;
    bool m_visible = false;
    bool m_runtimeKnown = false;
    bool m_pointerInside = false;
    bool m_obstructed = false;
    bool m_revealed = true;
    std::optional<Astrea::Typhon::Snapshot> m_runtimeSnapshot;
    QHash<QString, Astrea::Typhon::DockApplicationRuntimeState> m_runtimeStates;
    QHash<quint64, QString> m_pendingActivations;
    struct PendingWindowAction {
        QString desktopFileName;
        QString windowId;
        Astrea::Typhon::ToplevelAction action = Astrea::Typhon::ToplevelAction::Activate;
    };
    QHash<quint64, PendingWindowAction> m_pendingWindowActions;
    quint64 m_nextActivationToken = 0;
    TyphonToplevelConnection *m_typhonConnection = nullptr;
    QVector<QMetaObject::Connection> m_typhonConnections;
    Astrea::Typhon::DockApplicationStateProjector m_runtimeProjector;
    QString m_lastError;
    QTimer m_autoHideTimer;
};
