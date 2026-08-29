#include "services/DockConfigWatcher.hpp"

#include "services/DockConfigValidation.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonValue>

DockConfigWatcher::DockConfigWatcher(const QString &configPath, const QString &componentsPath,
                                     QObject *parent)
    : QObject(parent), m_configPath(QFileInfo(configPath).absoluteFilePath()),
      m_componentsPath(QFileInfo(componentsPath).absoluteFilePath())
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(120);
    connect(&m_debounce, &QTimer::timeout, this, &DockConfigWatcher::refresh);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this,
            [this](const QString &) { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
            [this](const QString &) { m_debounce.start(); });

    QDir().mkpath(QFileInfo(m_configPath).absolutePath());
    QDir().mkpath(QFileInfo(m_componentsPath).absolutePath());
    addPathWithParents(m_configPath);
    addPathWithParents(m_componentsPath);
    refresh();
}

bool DockConfigWatcher::componentEnabled() const
{
    const QJsonValue value = m_componentsConfig.value(QStringLiteral("dock"));
    return !value.isBool() || value.toBool();
}

void DockConfigWatcher::refresh()
{
    if (m_watcher.files().contains(m_configPath))
        m_watcher.removePath(m_configPath);
    if (m_watcher.files().contains(m_componentsPath))
        m_watcher.removePath(m_componentsPath);

    const JsonResult dockJson = loadJsonFile(m_configPath);
    const JsonResult componentsJson = loadJsonFile(m_componentsPath);
    QStringList errors;
    if (!dockJson.error.isEmpty())
        errors.append(dockJson.error);
    if (!componentsJson.error.isEmpty())
        errors.append(componentsJson.error);

    m_config = DockConfigCodec::parse(dockJson.object, &errors);
    m_componentsConfig = componentsJson.object;
    m_lastError = errors.join(QStringLiteral("; "));
    ++m_revision;

    addPathWithParents(m_configPath);
    addPathWithParents(m_componentsPath);

    emit configChanged();
    emit componentToggled(componentEnabled());
}

DockConfigWatcher::JsonResult DockConfigWatcher::loadJsonFile(const QString &path) const
{
    const DockConfigValidation::JsonResult result = DockConfigValidation::readJsonObject(path);
    return {result.object, result.error};
}

void DockConfigWatcher::addPathWithParents(const QString &path)
{
    const QFileInfo fileInfo(path);
    if (fileInfo.exists() && !m_watcher.files().contains(path))
        m_watcher.addPath(path);

    QDir directory = fileInfo.absoluteDir();
    if (directory.exists() && !m_watcher.directories().contains(directory.absolutePath()))
        m_watcher.addPath(directory.absolutePath());
    if (directory.cdUp() && directory.exists()
        && !m_watcher.directories().contains(directory.absolutePath()))
        m_watcher.addPath(directory.absolutePath());
}
