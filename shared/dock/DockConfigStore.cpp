#include "dock/DockConfigStore.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace {

bool fail(QString *errorOut, const QString &error)
{
    if (errorOut) {
        const QString bounded = error.trimmed().left(512);
        *errorOut = bounded.isEmpty() ? QStringLiteral("Dock configuration write failed")
                                      : bounded;
    }
    return false;
}

} // namespace

DockConfigStore::DockConfigStore(const QString &configPath)
    : m_configPath(QFileInfo(configPath).absoluteFilePath())
{
}

bool DockConfigStore::writePins(const QStringList &pins, QString *errorOut) const
{
    QString validationError;
    if (!DockConfigCodec::validatePinList(pins, &validationError))
        return fail(errorOut, validationError);

    const DockConfigCodec::JsonResult current =
        DockConfigCodec::readJsonObject(m_configPath);
    if (!current.error.isEmpty())
        return fail(errorOut, current.error);

    QJsonObject object = current.exists ? current.object : QJsonObject{};
    QJsonArray pinArray;
    for (const QString &pin : pins)
        pinArray.append(pin);
    object.insert(QStringLiteral("pins"), pinArray);
    return writeObject(object, errorOut);
}

bool DockConfigStore::writeConfig(const DockConfig &config, QString *errorOut) const
{
    QString validationError;
    if (!DockConfigCodec::validateConfig(config, &validationError))
        return fail(errorOut, validationError);

    const DockConfigCodec::JsonResult current =
        DockConfigCodec::readJsonObject(m_configPath);
    if (!current.error.isEmpty())
        return fail(errorOut, current.error);

    const QJsonObject source = current.exists ? current.object : QJsonObject{};
    return writeObject(DockConfigCodec::patchKnownFields(source, config, true), errorOut);
}

bool DockConfigStore::writePersonalization(const DockConfig &config, QString *errorOut) const
{
    DockConfig scalarConfig = config;
    scalarConfig.pins.clear();
    QString validationError;
    if (!DockConfigCodec::validateConfig(scalarConfig, &validationError))
        return fail(errorOut, validationError);

    const DockConfigCodec::JsonResult current =
        DockConfigCodec::readJsonObject(m_configPath);
    if (!current.error.isEmpty())
        return fail(errorOut, current.error);

    const QJsonObject source = current.exists ? current.object : QJsonObject{};
    return writeObject(DockConfigCodec::patchKnownFields(source, config, false), errorOut);
}

bool DockConfigStore::writeObject(const QJsonObject &object, QString *errorOut) const
{
    const QByteArray serialized = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (serialized.size() > DockConfigCodec::kMaximumConfigBytes)
        return fail(errorOut, QStringLiteral("Dock configuration exceeds the maximum size"));

    const QFileInfo fileInfo(m_configPath);
    if (!QDir().mkpath(fileInfo.absolutePath()))
        return fail(errorOut, QStringLiteral("Cannot create configuration directory"));

    QSaveFile file(m_configPath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return fail(errorOut, QStringLiteral("Cannot open Dock configuration for atomic write"));
    if (file.write(serialized) != serialized.size())
        return fail(errorOut, QStringLiteral("Cannot write Dock configuration atomically"));
    if (!file.commit())
        return fail(errorOut, QStringLiteral("Cannot commit Dock configuration atomically"));
    return true;
}
