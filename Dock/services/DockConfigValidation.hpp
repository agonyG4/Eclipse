#pragma once

#include "dock/DockConfig.hpp"

namespace DockConfigValidation {

inline constexpr qint64 kMaximumConfigBytes = DockConfigCodec::kMaximumConfigBytes;
inline constexpr int kMaximumPins = DockConfigCodec::kMaximumPins;
inline constexpr int kMaximumPinLength = DockConfigCodec::kMaximumPinLength;

using JsonResult = DockConfigCodec::JsonResult;
JsonResult readJsonObject(const QString &path);
bool validDesktopFileName(const QString &fileName);
bool validatePinList(const QStringList &pins, QString *errorOut = nullptr);

} // namespace DockConfigValidation
