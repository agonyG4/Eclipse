#include "WallpaperDescriptor.hpp"

#include <QJsonValue>

namespace Paper {
namespace {

QString normalized(const QString &value)
{
    return value.trimmed().toLower();
}

} // namespace

QString wallpaperKindToString(const WallpaperKind kind)
{
    switch (kind) {
    case WallpaperKind::Image:
        return QStringLiteral("image");
    case WallpaperKind::Dynamic:
        return QStringLiteral("dynamic");
    case WallpaperKind::Video:
        return QStringLiteral("video");
    case WallpaperKind::Slideshow:
        return QStringLiteral("slideshow");
    }
    return QStringLiteral("image");
}

WallpaperKind wallpaperKindFromString(const QString &value)
{
    const auto normalizedValue = normalized(value);
    if (normalizedValue == QStringLiteral("dynamic")) {
        return WallpaperKind::Dynamic;
    }
    if (normalizedValue == QStringLiteral("video")) {
        return WallpaperKind::Video;
    }
    if (normalizedValue == QStringLiteral("slideshow")) {
        return WallpaperKind::Slideshow;
    }
    return WallpaperKind::Image;
}

QString wallpaperSourceKindToString(const WallpaperSourceKind kind)
{
    switch (kind) {
    case WallpaperSourceKind::SystemResource:
        return QStringLiteral("system-resource");
    case WallpaperSourceKind::ExternalFile:
        return QStringLiteral("external-file");
    }
    return QStringLiteral("external-file");
}

WallpaperSourceKind wallpaperSourceKindFromString(const QString &value)
{
    if (normalized(value) == QStringLiteral("system-resource")) {
        return WallpaperSourceKind::SystemResource;
    }
    return WallpaperSourceKind::ExternalFile;
}

QString wallpaperOriginToString(const WallpaperOrigin origin)
{
    return origin == WallpaperOrigin::System ? QStringLiteral("system")
                                              : QStringLiteral("user");
}

WallpaperOrigin wallpaperOriginFromString(const QString &value)
{
    return normalized(value) == QStringLiteral("system") ? WallpaperOrigin::System
                                                            : WallpaperOrigin::User;
}

QString wallpaperFitToString(const WallpaperFit fit)
{
    switch (fit) {
    case WallpaperFit::Cover:
        return QStringLiteral("cover");
    case WallpaperFit::Contain:
        return QStringLiteral("contain");
    case WallpaperFit::Stretch:
        return QStringLiteral("stretch");
    case WallpaperFit::Center:
        return QStringLiteral("center");
    case WallpaperFit::Tile:
        return QStringLiteral("tile");
    }
    return QStringLiteral("cover");
}

WallpaperFit wallpaperFitFromString(const QString &value)
{
    const auto normalizedValue = normalized(value);
    if (normalizedValue == QStringLiteral("contain")) {
        return WallpaperFit::Contain;
    }
    if (normalizedValue == QStringLiteral("stretch")) {
        return WallpaperFit::Stretch;
    }
    if (normalizedValue == QStringLiteral("center")) {
        return WallpaperFit::Center;
    }
    if (normalizedValue == QStringLiteral("tile")) {
        return WallpaperFit::Tile;
    }
    return WallpaperFit::Cover;
}

std::optional<WallpaperFit> wallpaperFitFromStringStrict(const QString &value)
{
    const auto normalizedValue = normalized(value);
    if (normalizedValue == QStringLiteral("cover")) {
        return WallpaperFit::Cover;
    }
    if (normalizedValue == QStringLiteral("contain")) {
        return WallpaperFit::Contain;
    }
    if (normalizedValue == QStringLiteral("stretch")) {
        return WallpaperFit::Stretch;
    }
    if (normalizedValue == QStringLiteral("center")) {
        return WallpaperFit::Center;
    }
    if (normalizedValue == QStringLiteral("tile")) {
        return WallpaperFit::Tile;
    }
    return std::nullopt;
}

QString wallpaperScopeToString(const WallpaperScope scope)
{
    switch (scope) {
    case WallpaperScope::Global:
        return QStringLiteral("global");
    case WallpaperScope::Output:
        return QStringLiteral("output");
    }
    return QStringLiteral("global");
}

WallpaperScope wallpaperScopeFromString(const QString &value)
{
    return normalized(value) == QStringLiteral("output") ? WallpaperScope::Output
                                                            : WallpaperScope::Global;
}

WallpaperDescriptor WallpaperDescriptor::externalFile(const QString &source,
                                                       const WallpaperFit fit)
{
    WallpaperDescriptor descriptor;
    descriptor.m_sourceKind = WallpaperSourceKind::ExternalFile;
    descriptor.m_origin = WallpaperOrigin::User;
    descriptor.m_source = source;
    descriptor.m_logicalId = source;
    descriptor.m_fit = fit;
    return descriptor;
}

WallpaperDescriptor WallpaperDescriptor::systemResource(const QString &logicalId,
                                                        const QString &source,
                                                        const WallpaperFit fit)
{
    WallpaperDescriptor descriptor;
    descriptor.m_sourceKind = WallpaperSourceKind::SystemResource;
    descriptor.m_origin = WallpaperOrigin::System;
    descriptor.m_logicalId = logicalId;
    descriptor.m_source = source;
    descriptor.m_fit = fit;
    return descriptor;
}

WallpaperKind WallpaperDescriptor::kind() const
{
    return m_kind;
}

void WallpaperDescriptor::setKind(const WallpaperKind kind)
{
    m_kind = kind;
}

WallpaperSourceKind WallpaperDescriptor::sourceKind() const
{
    return m_sourceKind;
}

void WallpaperDescriptor::setSourceKind(const WallpaperSourceKind kind)
{
    m_sourceKind = kind;
}

WallpaperOrigin WallpaperDescriptor::origin() const
{
    return m_origin;
}

void WallpaperDescriptor::setOrigin(const WallpaperOrigin origin)
{
    m_origin = origin;
}

WallpaperFit WallpaperDescriptor::fit() const
{
    return m_fit;
}

void WallpaperDescriptor::setFit(const WallpaperFit fit)
{
    m_fit = fit;
}

WallpaperScope WallpaperDescriptor::scope() const
{
    return m_scope;
}

void WallpaperDescriptor::setScope(const WallpaperScope scope)
{
    m_scope = scope;
}

const QString &WallpaperDescriptor::logicalId() const
{
    return m_logicalId;
}

void WallpaperDescriptor::setLogicalId(const QString &logicalId)
{
    m_logicalId = logicalId;
}

const QString &WallpaperDescriptor::source() const
{
    return m_source;
}

void WallpaperDescriptor::setSource(const QString &source)
{
    m_source = source;
}

const QString &WallpaperDescriptor::resolvedSource() const
{
    return m_resolvedSource;
}

void WallpaperDescriptor::setResolvedSource(const QString &resolvedSource)
{
    m_resolvedSource = resolvedSource;
}

const QString &WallpaperDescriptor::displayName() const
{
    return m_displayName;
}

void WallpaperDescriptor::setDisplayName(const QString &displayName)
{
    m_displayName = displayName;
}

bool WallpaperDescriptor::isValid() const
{
    return !m_source.isEmpty() && m_kind == WallpaperKind::Image
        && (m_sourceKind == WallpaperSourceKind::SystemResource
            || m_sourceKind == WallpaperSourceKind::ExternalFile)
        && m_scope == WallpaperScope::Global;
}

QJsonObject WallpaperDescriptor::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("kind"), wallpaperKindToString(m_kind));
    json.insert(QStringLiteral("sourceKind"), wallpaperSourceKindToString(m_sourceKind));
    json.insert(QStringLiteral("origin"), wallpaperOriginToString(m_origin));
    json.insert(QStringLiteral("fit"), wallpaperFitToString(m_fit));
    json.insert(QStringLiteral("scope"), wallpaperScopeToString(m_scope));
    json.insert(QStringLiteral("logicalId"), m_logicalId);
    json.insert(QStringLiteral("source"), m_source);
    if (!m_displayName.isEmpty()) {
        json.insert(QStringLiteral("displayName"), m_displayName);
    }
    if (!m_resolvedSource.isEmpty()) {
        json.insert(QStringLiteral("resolvedSource"), m_resolvedSource);
    }
    return json;
}

WallpaperDescriptor WallpaperDescriptor::fromJson(const QJsonObject &json)
{
    WallpaperDescriptor descriptor;
    descriptor.m_kind = wallpaperKindFromString(json.value(QStringLiteral("kind")).toString());
    descriptor.m_sourceKind = wallpaperSourceKindFromString(
        json.value(QStringLiteral("sourceKind")).toString());
    descriptor.m_origin = json.contains(QStringLiteral("origin"))
        ? wallpaperOriginFromString(json.value(QStringLiteral("origin")).toString())
        : descriptor.m_sourceKind == WallpaperSourceKind::SystemResource
            ? WallpaperOrigin::System
            : WallpaperOrigin::User;
    descriptor.m_fit = wallpaperFitFromString(json.value(QStringLiteral("fit")).toString());
    descriptor.m_scope = wallpaperScopeFromString(json.value(QStringLiteral("scope")).toString());
    descriptor.m_logicalId = json.value(QStringLiteral("logicalId")).toString();
    descriptor.m_source = json.value(QStringLiteral("source")).toString();
    descriptor.m_resolvedSource = json.value(QStringLiteral("resolvedSource")).toString();
    descriptor.m_displayName = json.value(QStringLiteral("displayName")).toString();
    return descriptor;
}

bool operator==(const WallpaperDescriptor &lhs, const WallpaperDescriptor &rhs)
{
    return lhs.m_kind == rhs.m_kind && lhs.m_sourceKind == rhs.m_sourceKind
        && lhs.m_origin == rhs.m_origin
        && lhs.m_fit == rhs.m_fit && lhs.m_scope == rhs.m_scope
        && lhs.m_logicalId == rhs.m_logicalId && lhs.m_source == rhs.m_source
        && lhs.m_resolvedSource == rhs.m_resolvedSource
        && lhs.m_displayName == rhs.m_displayName;
}

} // namespace Paper
