#include "services/DockConfigValidation.hpp"

namespace DockConfigValidation {

JsonResult readJsonObject(const QString &path)
{
    return DockConfigCodec::readJsonObject(path);
}

bool validDesktopFileName(const QString &fileName)
{
    return DockConfigCodec::validDesktopFileName(fileName);
}

bool validatePinList(const QStringList &pins, QString *errorOut)
{
    return DockConfigCodec::validatePinList(pins, errorOut);
}

} // namespace DockConfigValidation
