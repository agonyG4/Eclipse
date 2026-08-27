#include "services/DockConfigValidation.hpp"

#include <QChar>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>

namespace DockConfigValidation {

JsonResult readJsonObject(const QString &path)
{
    QFile file(path);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {{}, true, QStringLiteral("Cannot read %1").arg(path)};
    if (file.size() > kMaximumConfigBytes) {
        return {{}, true, QStringLiteral("Configuration file is too large: %1").arg(path)};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isObject()) {
        return {{}, true,
                QStringLiteral("Invalid JSON in %1: %2").arg(path, parseError.errorString())};
    }
    return {document.object(), true, {}};
}

bool validDesktopFileName(const QString &fileName)
{
    return !fileName.isEmpty() && fileName.size() <= kMaximumPinLength
        && fileName.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)
        && !fileName.contains(QLatin1Char('/'))
        && !fileName.contains(QLatin1Char('\\'))
        && !fileName.contains(QChar::Null)
        && !fileName.contains(QStringLiteral(".."));
}

bool validatePinList(const QStringList &pins, QString *errorOut)
{
    if (pins.size() > kMaximumPins) {
        if (errorOut)
            *errorOut = QStringLiteral("pins contains too many entries");
        return false;
    }

    QStringList seen;
    seen.reserve(pins.size());
    for (const QString &pin : pins) {
        if (!validDesktopFileName(pin)) {
            if (errorOut)
                *errorOut = QStringLiteral("invalid desktop filename in pins: %1").arg(pin);
            return false;
        }
        if (seen.contains(pin)) {
            if (errorOut)
                *errorOut = QStringLiteral("duplicate desktop filename in pins: %1").arg(pin);
            return false;
        }
        seen.append(pin);
    }
    return true;
}

} // namespace DockConfigValidation
