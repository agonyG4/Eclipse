#include "services/DockConfigWatcher.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QDebug>
#include <QtMath>

namespace {
constexpr qint64 kMaximumConfigBytes = 1024LL * 1024LL;
constexpr int kMaximumPins = 256;
constexpr int kMaximumPinLength = 255;
}

DockConfig DockConfig::defaults()
{
    return {};
}

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

    m_config = parseConfig(dockJson.object, &errors);
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
    QFile file(path);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {{}, QStringLiteral("Cannot read %1").arg(path)};
    if (file.size() > kMaximumConfigBytes)
        return {{}, QStringLiteral("Configuration file is too large: %1").arg(path)};

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isObject())
        return {{}, QStringLiteral("Invalid JSON in %1: %2").arg(path, parseError.errorString())};
    return {document.object(), {}};
}

DockConfig DockConfigWatcher::parseConfig(const QJsonObject &object, QStringList *errors) const
{
    DockConfig result = DockConfig::defaults();
    result.iconSize = integerField(object, QStringLiteral("iconSize"), result.iconSize, 32, 64, errors);
    result.bottomMargin = integerField(object, QStringLiteral("bottomMargin"), result.bottomMargin, 0, 48, errors);
    result.panelPadding = integerField(object, QStringLiteral("panelPadding"), result.panelPadding, 8, 32, errors);
    result.itemSpacing = integerField(object, QStringLiteral("itemSpacing"), result.itemSpacing, 4, 24, errors);

    if (!object.contains(QStringLiteral("pins")))
        return result;
    const QJsonValue pinsValue = object.value(QStringLiteral("pins"));
    if (!pinsValue.isArray()) {
        errors->append(QStringLiteral("pins must be an array"));
        return result;
    }

    const QJsonArray pins = pinsValue.toArray();
    if (pins.size() > kMaximumPins) {
        errors->append(QStringLiteral("pins contains too many entries"));
        return result;
    }
    for (const auto value : pins) {
        if (!value.isString()) {
            errors->append(QStringLiteral("pins entries must be strings"));
            continue;
        }
        const QString pin = value.toString();
        if (!validDesktopFileName(pin)) {
            errors->append(QStringLiteral("invalid desktop filename in pins: %1").arg(pin));
            continue;
        }
        if (!result.pins.contains(pin))
            result.pins.append(pin);
    }
    return result;
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

bool DockConfigWatcher::validDesktopFileName(const QString &fileName)
{
    return !fileName.isEmpty() && fileName.size() <= kMaximumPinLength
        && fileName.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)
        && !fileName.contains(QLatin1Char('/'))
        && !fileName.contains(QLatin1Char('\\'))
        && !fileName.contains(QChar::Null)
        && !fileName.contains(QStringLiteral(".."));
}

int DockConfigWatcher::integerField(const QJsonObject &object, const QString &key, int fallback,
                                    int minimum, int maximum, QStringList *errors)
{
    if (!object.contains(key))
        return fallback;
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        errors->append(QStringLiteral("%1 must be numeric").arg(key));
        return fallback;
    }
    const double number = value.toDouble();
    if (!qIsFinite(number)) {
        errors->append(QStringLiteral("%1 must be finite").arg(key));
        return fallback;
    }

    const double bounded = qBound(static_cast<double>(minimum), number,
                                  static_cast<double>(maximum));
    return qRound(bounded);
}
