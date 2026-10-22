#pragma once
#include <QVariantMap>
namespace RegionSettings
{
QVariantMap read(const QString &path);
QVariantMap update(const QString &path, const QString &key, const QVariant &value);
QVariantMap snapshot(const QString &path);
QString effectiveTimeFormat(const QVariantMap &region);
} // namespace RegionSettings
