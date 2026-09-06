#pragma once

#include "dock/DockConfig.hpp"

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

class SettingsDockController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(int panelPadding READ panelPadding WRITE setPanelPadding NOTIFY panelPaddingChanged)
    Q_PROPERTY(int itemSpacing READ itemSpacing WRITE setItemSpacing NOTIFY itemSpacingChanged)
    Q_PROPERTY(QString hoverEffect READ hoverEffect WRITE setHoverEffect NOTIFY hoverEffectChanged)
    Q_PROPERTY(double magnificationScale READ magnificationScale WRITE setMagnificationScale NOTIFY magnificationScaleChanged)
    Q_PROPERTY(double magnificationRadius READ magnificationRadius WRITE setMagnificationRadius NOTIFY magnificationRadiusChanged)
    Q_PROPERTY(int edgeMargin READ edgeMargin WRITE setEdgeMargin NOTIFY edgeMarginChanged)
    Q_PROPERTY(QString position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(bool floating READ floating WRITE setFloating NOTIFY floatingChanged)
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(QString autoHide READ autoHide WRITE setAutoHide NOTIFY autoHideChanged)
    Q_PROPERTY(QString indicatorStyle READ indicatorStyle WRITE setIndicatorStyle NOTIFY indicatorStyleChanged)
    Q_PROPERTY(int indicatorSize READ indicatorSize WRITE setIndicatorSize NOTIFY indicatorSizeChanged)
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled WRITE setAnimationsEnabled NOTIFY animationsEnabledChanged)
    Q_PROPERTY(double animationSpeed READ animationSpeed WRITE setAnimationSpeed NOTIFY animationSpeedChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool pendingWrite READ pendingWrite NOTIFY pendingWriteChanged)
    Q_PROPERTY(bool isDefault READ isDefault NOTIFY isDefaultChanged)
    Q_PROPERTY(int defaultIconSize READ defaultIconSize CONSTANT)
    Q_PROPERTY(int defaultPanelPadding READ defaultPanelPadding CONSTANT)
    Q_PROPERTY(int defaultItemSpacing READ defaultItemSpacing CONSTANT)
    Q_PROPERTY(double defaultMagnificationScale READ defaultMagnificationScale CONSTANT)
    Q_PROPERTY(double defaultMagnificationRadius READ defaultMagnificationRadius CONSTANT)
    Q_PROPERTY(int defaultEdgeMargin READ defaultEdgeMargin CONSTANT)
    Q_PROPERTY(int defaultCornerRadius READ defaultCornerRadius CONSTANT)
    Q_PROPERTY(int defaultIndicatorSize READ defaultIndicatorSize CONSTANT)
    Q_PROPERTY(double defaultAnimationSpeed READ defaultAnimationSpeed CONSTANT)
    Q_PROPERTY(bool canUndoRestore READ canUndoRestore NOTIFY canUndoRestoreChanged)

public:
    explicit SettingsDockController(const QString &configPath = {}, QObject *parent = nullptr);
    ~SettingsDockController() override;

    QString configPath() const { return m_configPath; }
    int iconSize() const { return m_config.iconSize; }
    int panelPadding() const { return m_config.panelPadding; }
    int itemSpacing() const { return m_config.itemSpacing; }
    QString hoverEffect() const { return m_config.hoverEffect; }
    double magnificationScale() const { return m_config.magnificationScale; }
    double magnificationRadius() const { return m_config.magnificationRadius; }
    int edgeMargin() const { return m_config.edgeMargin; }
    QString position() const { return m_config.position; }
    bool floating() const { return m_config.floating; }
    int cornerRadius() const { return m_config.cornerRadius; }
    QString autoHide() const { return m_config.autoHide; }
    QString indicatorStyle() const { return m_config.indicatorStyle; }
    int indicatorSize() const { return m_config.indicatorSize; }
    bool animationsEnabled() const { return m_config.animationsEnabled; }
    double animationSpeed() const { return m_config.animationSpeed; }
    QString lastError() const { return m_lastError; }
    bool pendingWrite() const { return m_pendingWrite; }
    bool isDefault() const { return m_isDefault; }
    int defaultIconSize() const { return DockConfig::defaults().iconSize; }
    int defaultPanelPadding() const { return DockConfig::defaults().panelPadding; }
    int defaultItemSpacing() const { return DockConfig::defaults().itemSpacing; }
    double defaultMagnificationScale() const { return DockConfig::defaults().magnificationScale; }
    double defaultMagnificationRadius() const { return DockConfig::defaults().magnificationRadius; }
    int defaultEdgeMargin() const { return DockConfig::defaults().edgeMargin; }
    int defaultCornerRadius() const { return DockConfig::defaults().cornerRadius; }
    int defaultIndicatorSize() const { return DockConfig::defaults().indicatorSize; }
    double defaultAnimationSpeed() const { return DockConfig::defaults().animationSpeed; }
    bool canUndoRestore() const { return m_canUndoRestore; }

public slots:
    void setIconSize(int value);
    void setPanelPadding(int value);
    void setItemSpacing(int value);
    void setHoverEffect(const QString &value);
    void setMagnificationScale(double value);
    void setMagnificationRadius(double value);
    void setEdgeMargin(int value);
    void setPosition(const QString &value);
    void setFloating(bool value);
    void setCornerRadius(int value);
    void setAutoHide(const QString &value);
    void setIndicatorStyle(const QString &value);
    void setIndicatorSize(int value);
    void setAnimationsEnabled(bool value);
    void setAnimationSpeed(double value);
    void refresh();
    Q_INVOKABLE void flush();
    Q_INVOKABLE void restoreDefaults();
    Q_INVOKABLE void undoRestore();

signals:
    void iconSizeChanged();
    void panelPaddingChanged();
    void itemSpacingChanged();
    void hoverEffectChanged();
    void magnificationScaleChanged();
    void magnificationRadiusChanged();
    void edgeMarginChanged();
    void positionChanged();
    void floatingChanged();
    void cornerRadiusChanged();
    void autoHideChanged();
    void indicatorStyleChanged();
    void indicatorSizeChanged();
    void animationsEnabledChanged();
    void animationSpeedChanged();
    void configChanged();
    void lastErrorChanged();
    void pendingWriteChanged();
    void isDefaultChanged();
    void canUndoRestoreChanged();

private:
    static bool samePersonalization(const DockConfig &left, const DockConfig &right);
    static bool sameConfig(const DockConfig &left, const DockConfig &right);
    void applyLocalConfig(const DockConfig &config);
    void scheduleWrite();
    void setLastError(const QString &error);
    void addPathWithParents(const QString &path);
    void setFieldConfig(const DockConfig &config);
    bool refreshFromDisk();
    bool flushPendingWrite();
    void clearUndoRestore();

    QString m_configPath;
    DockConfig m_config;
    DockConfig m_lastPersisted;
    QString m_lastError;
    bool m_pendingWrite = false;
    bool m_isDefault = true;
    bool m_canUndoRestore = false;
    DockConfig m_restoreSnapshot;
    QFileSystemWatcher m_watcher;
    QTimer m_refreshDebounce;
    QTimer m_writeDebounce;
    QTimer m_restoreUndoTimer;
};
