#include "services/AltTabConfigWatcher.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QDir>
#include <QFileInfo>

AltTabConfigWatcher::AltTabConfigWatcher(const QString &alttabConfigPath, const QString &componentsPath,
                                         QObject *parent)
    : QObject(parent), m_configPath(alttabConfigPath), m_componentsPath(componentsPath)
{
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(150);
    connect(m_debounce, &QTimer::timeout, this, &AltTabConfigWatcher::refresh);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        m_debounce->start();
    });
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        m_debounce->start();
    });

    QDir().mkpath(QFileInfo(alttabConfigPath).absolutePath());
    QDir().mkpath(QFileInfo(componentsPath).absolutePath());

    addPathWithParents(alttabConfigPath);
    addPathWithParents(componentsPath);

    refresh();
}

void AltTabConfigWatcher::addPathWithParents(const QString &path) {
    if (QFileInfo::exists(path))
        m_watcher->addPath(path);
    QDir dir = QFileInfo(path).absoluteDir();
    const QString dirPath = dir.absolutePath();
    if (!m_watcher->directories().contains(dirPath) && dir.exists())
        m_watcher->addPath(dirPath);
    if (dir.cdUp()) {
        const QString parentPath = dir.absolutePath();
        if (!m_watcher->directories().contains(parentPath) && dir.exists())
            m_watcher->addPath(parentPath);
    }
}

void AltTabConfigWatcher::refresh() {
    if (m_watcher->files().contains(m_configPath))
        m_watcher->removePath(m_configPath);
    if (m_watcher->files().contains(m_componentsPath))
        m_watcher->removePath(m_componentsPath);

    m_alttabConfig = loadJsonFile(m_configPath);
    m_componentsConfig = loadJsonFile(m_componentsPath);

    addPathWithParents(m_configPath);
    addPathWithParents(m_componentsPath);

    emit configChanged();
    emit componentToggled(componentEnabled());
}

bool AltTabConfigWatcher::componentEnabled() const {
    if (m_componentsConfig.isEmpty())
        return true;
    QJsonValue val = m_componentsConfig.value(QStringLiteral("alttab"));
    if (val.isBool())
        return val.toBool();
    return true;
}

QJsonObject AltTabConfigWatcher::loadJsonFile(const QString &path) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return {};
    return doc.object();
}
