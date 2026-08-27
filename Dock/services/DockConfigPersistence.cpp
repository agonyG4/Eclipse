#include "services/DockConfigPersistence.hpp"

#include "services/DockConfigValidation.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace {

QString boundedError(const QString &error)
{
    const QString trimmed = error.trimmed();
    return trimmed.left(512).isEmpty() ? QStringLiteral("Dock configuration write failed")
                                      : trimmed.left(512);
}

bool fail(QString *errorOut, const QString &error)
{
    if (errorOut)
        *errorOut = boundedError(error);
    return false;
}

} // namespace

DockConfigPersistence::DockConfigPersistence(const QString &configPath)
    : m_configPath(QFileInfo(configPath).absoluteFilePath())
{
}

bool DockConfigPersistence::writePins(const QStringList &pins, QString *errorOut)
{
    QString validationError;
    if (!DockConfigValidation::validatePinList(pins, &validationError))
        return fail(errorOut, validationError);

    const DockConfigValidation::JsonResult current =
        DockConfigValidation::readJsonObject(m_configPath);
    if (!current.error.isEmpty())
        return fail(errorOut, current.error);

    QJsonObject object = current.exists ? current.object : QJsonObject{};
    QJsonArray pinArray;
    for (const QString &pin : pins)
        pinArray.append(pin);
    object.insert(QStringLiteral("pins"), pinArray);

    const QByteArray serialized = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (serialized.size() > DockConfigValidation::kMaximumConfigBytes) {
        return fail(errorOut, QStringLiteral("Dock configuration exceeds the maximum size"));
    }

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
