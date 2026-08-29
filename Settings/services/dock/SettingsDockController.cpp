#include "services/dock/SettingsDockController.hpp"

#include "dock/DockConfigStore.hpp"

#include <QDir>
#include <QFileInfo>

#include <QtMath>

namespace {

QString productionConfigPath()
{
    return QDir::homePath() + QStringLiteral("/.config/AstreaOS/dock.json");
}

bool equalReal(double left, double right)
{
    return qFuzzyCompare(left + 1.0, right + 1.0);
}

} // namespace

SettingsDockController::SettingsDockController(const QString &configPath, QObject *parent)
    : QObject(parent)
    , m_configPath(QFileInfo(configPath.isEmpty() ? productionConfigPath() : configPath)
                       .absoluteFilePath())
    , m_config(DockConfig::defaults())
    , m_lastPersisted(m_config)
{
    m_refreshDebounce.setSingleShot(true);
    m_refreshDebounce.setInterval(100);
    connect(&m_refreshDebounce, &QTimer::timeout, this, &SettingsDockController::refresh);

    m_writeDebounce.setSingleShot(true);
    m_writeDebounce.setInterval(180);
    connect(&m_writeDebounce, &QTimer::timeout, this, &SettingsDockController::flush);

    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this,
            [this](const QString &) {
                addPathWithParents(m_configPath);
                m_refreshDebounce.start();
            });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
            [this](const QString &) { m_refreshDebounce.start(); });

    addPathWithParents(m_configPath);
    refresh();
}

SettingsDockController::~SettingsDockController()
{
    flush();
}

bool SettingsDockController::sameConfig(const DockConfig &left, const DockConfig &right)
{
    return left.iconSize == right.iconSize && left.panelPadding == right.panelPadding
        && left.itemSpacing == right.itemSpacing && left.hoverEffect == right.hoverEffect
        && equalReal(left.magnificationScale, right.magnificationScale)
        && equalReal(left.magnificationRadius, right.magnificationRadius)
        && left.edgeMargin == right.edgeMargin && left.position == right.position
        && left.floating == right.floating && left.cornerRadius == right.cornerRadius
        && left.autoHide == right.autoHide && left.indicatorStyle == right.indicatorStyle
        && left.indicatorSize == right.indicatorSize
        && left.animationsEnabled == right.animationsEnabled
        && equalReal(left.animationSpeed, right.animationSpeed) && left.pins == right.pins;
}

void SettingsDockController::applyLocalConfig(const DockConfig &config)
{
    const DockConfig previous = m_config;
    m_config = config;
    m_config.bottomMargin = m_config.edgeMargin;

    if (previous.iconSize != m_config.iconSize)
        emit iconSizeChanged();
    if (previous.panelPadding != m_config.panelPadding)
        emit panelPaddingChanged();
    if (previous.itemSpacing != m_config.itemSpacing)
        emit itemSpacingChanged();
    if (previous.hoverEffect != m_config.hoverEffect)
        emit hoverEffectChanged();
    if (!equalReal(previous.magnificationScale, m_config.magnificationScale))
        emit magnificationScaleChanged();
    if (!equalReal(previous.magnificationRadius, m_config.magnificationRadius))
        emit magnificationRadiusChanged();
    if (previous.edgeMargin != m_config.edgeMargin)
        emit edgeMarginChanged();
    if (previous.position != m_config.position)
        emit positionChanged();
    if (previous.floating != m_config.floating)
        emit floatingChanged();
    if (previous.cornerRadius != m_config.cornerRadius)
        emit cornerRadiusChanged();
    if (previous.autoHide != m_config.autoHide)
        emit autoHideChanged();
    if (previous.indicatorStyle != m_config.indicatorStyle)
        emit indicatorStyleChanged();
    if (previous.indicatorSize != m_config.indicatorSize)
        emit indicatorSizeChanged();
    if (previous.animationsEnabled != m_config.animationsEnabled)
        emit animationsEnabledChanged();
    if (!equalReal(previous.animationSpeed, m_config.animationSpeed))
        emit animationSpeedChanged();
    if (!sameConfig(previous, m_config))
        emit configChanged();
}

void SettingsDockController::setLastError(const QString &error)
{
    const QString bounded = error.trimmed().left(512);
    if (m_lastError == bounded)
        return;
    m_lastError = bounded;
    emit lastErrorChanged();
}

void SettingsDockController::setFieldConfig(const DockConfig &config)
{
    if (sameConfig(m_config, config))
        return;
    applyLocalConfig(config);
    scheduleWrite();
}

void SettingsDockController::scheduleWrite()
{
    if (!m_pendingWrite) {
        m_pendingWrite = true;
        emit pendingWriteChanged();
    }
    m_writeDebounce.start();
}

void SettingsDockController::addPathWithParents(const QString &path)
{
    const QFileInfo fileInfo(path);
    if (fileInfo.exists()) {
        if (!m_watcher.files().contains(path))
            m_watcher.addPath(path);
    }

    QString directory = fileInfo.absolutePath();
    while (!directory.isEmpty() && !QDir(directory).exists()) {
        const QString parent = QFileInfo(directory).absolutePath();
        if (parent == directory)
            break;
        directory = parent;
    }
    if (!directory.isEmpty() && !m_watcher.directories().contains(directory))
        m_watcher.addPath(directory);
}

void SettingsDockController::refresh()
{
    const DockConfigCodec::JsonResult json = DockConfigCodec::readJsonObject(m_configPath);
    if (!json.error.isEmpty()) {
        setLastError(json.error);
        addPathWithParents(m_configPath);
        return;
    }

    QStringList diagnostics;
    const DockConfig next = DockConfigCodec::parse(json.exists ? json.object : QJsonObject{},
                                                   &diagnostics);
    applyLocalConfig(next);
    m_lastPersisted = next;
    setLastError(diagnostics.join(QStringLiteral("; ")));
    addPathWithParents(m_configPath);
}

void SettingsDockController::flush()
{
    m_writeDebounce.stop();
    if (!m_pendingWrite)
        return;

    DockConfigStore store(m_configPath);
    QString error;
    if (!store.writeConfig(m_config, &error)) {
        applyLocalConfig(m_lastPersisted);
        setLastError(error);
    } else {
        m_lastPersisted = m_config;
        setLastError(QString());
    }

    m_pendingWrite = false;
    emit pendingWriteChanged();
    addPathWithParents(m_configPath);
}

void SettingsDockController::setIconSize(int value)
{
    DockConfig next = m_config;
    next.iconSize = DockConfigCodec::parse(QJsonObject{{QStringLiteral("iconSize"), value}}).iconSize;
    setFieldConfig(next);
}

void SettingsDockController::setPanelPadding(int value)
{
    DockConfig next = m_config;
    next.panelPadding = DockConfigCodec::parse(QJsonObject{{QStringLiteral("panelPadding"), value}})
                            .panelPadding;
    setFieldConfig(next);
}

void SettingsDockController::setItemSpacing(int value)
{
    DockConfig next = m_config;
    next.itemSpacing = DockConfigCodec::parse(QJsonObject{{QStringLiteral("itemSpacing"), value}})
                           .itemSpacing;
    setFieldConfig(next);
}

void SettingsDockController::setHoverEffect(const QString &value)
{
    DockConfig next = m_config;
    next.hoverEffect = DockConfigCodec::parse(
                           QJsonObject{{QStringLiteral("hoverEffect"), value}})
                           .hoverEffect;
    setFieldConfig(next);
}

void SettingsDockController::setMagnificationScale(double value)
{
    DockConfig next = m_config;
    next.magnificationScale = DockConfigCodec::parse(
                                  QJsonObject{{QStringLiteral("magnificationScale"), value}})
                                  .magnificationScale;
    setFieldConfig(next);
}

void SettingsDockController::setMagnificationRadius(double value)
{
    DockConfig next = m_config;
    next.magnificationRadius = DockConfigCodec::parse(
                                   QJsonObject{{QStringLiteral("magnificationRadius"), value}})
                                   .magnificationRadius;
    setFieldConfig(next);
}

void SettingsDockController::setEdgeMargin(int value)
{
    DockConfig next = m_config;
    next.edgeMargin = DockConfigCodec::parse(QJsonObject{{QStringLiteral("edgeMargin"), value}})
                          .edgeMargin;
    setFieldConfig(next);
}

void SettingsDockController::setPosition(const QString &value)
{
    DockConfig next = m_config;
    next.position = DockConfigCodec::parse(QJsonObject{{QStringLiteral("position"), value}})
                        .position;
    setFieldConfig(next);
}

void SettingsDockController::setFloating(bool value)
{
    DockConfig next = m_config;
    next.floating = DockConfigCodec::parse(QJsonObject{{QStringLiteral("floating"), value}})
                        .floating;
    setFieldConfig(next);
}

void SettingsDockController::setCornerRadius(int value)
{
    DockConfig next = m_config;
    next.cornerRadius = DockConfigCodec::parse(QJsonObject{{QStringLiteral("cornerRadius"), value}})
                            .cornerRadius;
    setFieldConfig(next);
}

void SettingsDockController::setAutoHide(const QString &value)
{
    DockConfig next = m_config;
    next.autoHide = DockConfigCodec::parse(QJsonObject{{QStringLiteral("autoHide"), value}})
                        .autoHide;
    setFieldConfig(next);
}

void SettingsDockController::setIndicatorStyle(const QString &value)
{
    DockConfig next = m_config;
    next.indicatorStyle = DockConfigCodec::parse(
                              QJsonObject{{QStringLiteral("indicatorStyle"), value}})
                              .indicatorStyle;
    setFieldConfig(next);
}

void SettingsDockController::setIndicatorSize(int value)
{
    DockConfig next = m_config;
    next.indicatorSize = DockConfigCodec::parse(
                             QJsonObject{{QStringLiteral("indicatorSize"), value}})
                             .indicatorSize;
    setFieldConfig(next);
}

void SettingsDockController::setAnimationsEnabled(bool value)
{
    DockConfig next = m_config;
    next.animationsEnabled = DockConfigCodec::parse(
                                 QJsonObject{{QStringLiteral("animationsEnabled"), value}})
                                 .animationsEnabled;
    setFieldConfig(next);
}

void SettingsDockController::setAnimationSpeed(double value)
{
    DockConfig next = m_config;
    next.animationSpeed = DockConfigCodec::parse(
                              QJsonObject{{QStringLiteral("animationSpeed"), value}})
                              .animationSpeed;
    setFieldConfig(next);
}
