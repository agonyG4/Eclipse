#include "services/SpotlightConfigWatcher.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

SpotlightConfigWatcher::SpotlightConfigWatcher(const QString &configPath, const QString &componentsPath,
                                               QObject *parent)
    : QObject(parent), m_configPath(configPath), m_componentsPath(componentsPath) {
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(150);
    connect(m_debounce, &QTimer::timeout, this, &SpotlightConfigWatcher::refresh);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        m_debounce->start();
    });
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        m_debounce->start();
    });

    QDir().mkpath(QFileInfo(configPath).absolutePath());
    QDir().mkpath(QFileInfo(componentsPath).absolutePath());

    addPathWithParents(configPath);
    addPathWithParents(componentsPath);

    refresh();
}

void SpotlightConfigWatcher::addPathWithParents(const QString &path) {
    if (QFileInfo::exists(path))
        m_watcher->addPath(path);
    QDir dir = QFileInfo(path).absoluteDir();
    QString dirPath = dir.absolutePath();
    if (!m_watcher->directories().contains(dirPath) && dir.exists())
        m_watcher->addPath(dirPath);
    // Also watch parent for atomic replacement (rename over)
    if (dir.cdUp()) {
        QString parentPath = dir.absolutePath();
        if (!m_watcher->directories().contains(parentPath) && dir.exists())
            m_watcher->addPath(parentPath);
    }
}

void SpotlightConfigWatcher::refresh() {
    // Remove and re-add file paths to handle atomic replacement (inode change)
    if (m_watcher->files().contains(m_configPath))
        m_watcher->removePath(m_configPath);
    if (m_watcher->files().contains(m_componentsPath))
        m_watcher->removePath(m_componentsPath);

    m_spotlightConfig = loadJsonFile(m_configPath);
    m_componentsConfig = loadJsonFile(m_componentsPath);

    addPathWithParents(m_configPath);
    addPathWithParents(m_componentsPath);

    emit configChanged();
    emit componentToggled(componentEnabled());
}

bool SpotlightConfigWatcher::componentEnabled() const {
    if (m_componentsConfig.isEmpty())
        return true;
    QJsonValue val = m_componentsConfig.value(QStringLiteral("spotlight"));
    if (val.isBool())
        return val.toBool();
    return true;
}

bool SpotlightConfigWatcher::weatherEnabled() const {
    if (m_spotlightConfig.isEmpty())
        return true;
    QJsonValue val = m_spotlightConfig.value(QStringLiteral("weather"));
    if (val.isBool())
        return val.toBool();
    return true;
}

QJsonObject SpotlightConfigWatcher::loadJsonFile(const QString &path) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return {};
    return doc.object();
}
