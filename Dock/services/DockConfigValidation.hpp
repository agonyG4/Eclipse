#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace DockConfigValidation {

inline constexpr qint64 kMaximumConfigBytes = 1024LL * 1024LL;
inline constexpr int kMaximumPins = 256;
inline constexpr int kMaximumPinLength = 255;

struct JsonResult {
    QJsonObject object;
    bool exists = false;
    QString error;
};

JsonResult readJsonObject(const QString &path);
bool validDesktopFileName(const QString &fileName);
bool validatePinList(const QStringList &pins, QString *errorOut = nullptr);

} // namespace DockConfigValidation
