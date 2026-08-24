#pragma once

#include <QJsonObject>
#include <QMetaType>
#include <QString>

#include <optional>

namespace Paper {

Q_NAMESPACE

enum class WallpaperKind {
    Image,
    Dynamic,
    Video,
    Slideshow,
};
Q_ENUM_NS(WallpaperKind)

enum class WallpaperSourceKind {
    SystemResource,
    ExternalFile,
};
Q_ENUM_NS(WallpaperSourceKind)

enum class WallpaperOrigin {
    System,
    User,
};
Q_ENUM_NS(WallpaperOrigin)

enum class WallpaperFit {
    Cover,
    Contain,
    Stretch,
    Center,
    Tile,
};
Q_ENUM_NS(WallpaperFit)

enum class WallpaperScope {
    Global,
    Output,
};
Q_ENUM_NS(WallpaperScope)

QString wallpaperKindToString(WallpaperKind kind);
WallpaperKind wallpaperKindFromString(const QString &value);

QString wallpaperSourceKindToString(WallpaperSourceKind kind);
WallpaperSourceKind wallpaperSourceKindFromString(const QString &value);

QString wallpaperOriginToString(WallpaperOrigin origin);
WallpaperOrigin wallpaperOriginFromString(const QString &value);

QString wallpaperFitToString(WallpaperFit fit);
WallpaperFit wallpaperFitFromString(const QString &value);
std::optional<WallpaperFit> wallpaperFitFromStringStrict(const QString &value);

QString wallpaperScopeToString(WallpaperScope scope);
WallpaperScope wallpaperScopeFromString(const QString &value);

class WallpaperDescriptor final
{
    Q_GADGET
    Q_PROPERTY(WallpaperKind kind READ kind WRITE setKind)
    Q_PROPERTY(WallpaperSourceKind sourceKind READ sourceKind WRITE setSourceKind)
    Q_PROPERTY(WallpaperOrigin origin READ origin WRITE setOrigin)
    Q_PROPERTY(WallpaperFit fit READ fit WRITE setFit)
    Q_PROPERTY(WallpaperScope scope READ scope WRITE setScope)
    Q_PROPERTY(QString logicalId READ logicalId WRITE setLogicalId)
    Q_PROPERTY(QString source READ source WRITE setSource)
    Q_PROPERTY(QString resolvedSource READ resolvedSource WRITE setResolvedSource)
    Q_PROPERTY(QString displayName READ displayName WRITE setDisplayName)

public:
    static WallpaperDescriptor externalFile(const QString &source, WallpaperFit fit);
    static WallpaperDescriptor systemResource(const QString &logicalId,
                                              const QString &source,
                                              WallpaperFit fit);

    WallpaperKind kind() const;
    void setKind(WallpaperKind kind);

    WallpaperSourceKind sourceKind() const;
    void setSourceKind(WallpaperSourceKind kind);

    WallpaperOrigin origin() const;
    void setOrigin(WallpaperOrigin origin);

    WallpaperFit fit() const;
    void setFit(WallpaperFit fit);

    WallpaperScope scope() const;
    void setScope(WallpaperScope scope);

    const QString &logicalId() const;
    void setLogicalId(const QString &logicalId);

    const QString &source() const;
    void setSource(const QString &source);

    const QString &resolvedSource() const;
    void setResolvedSource(const QString &resolvedSource);

    const QString &displayName() const;
    void setDisplayName(const QString &displayName);

    bool isValid() const;
    QJsonObject toJson() const;
    static WallpaperDescriptor fromJson(const QJsonObject &json);

    friend bool operator==(const WallpaperDescriptor &lhs,
                           const WallpaperDescriptor &rhs);
    friend bool operator!=(const WallpaperDescriptor &lhs,
                           const WallpaperDescriptor &rhs)
    {
        return !(lhs == rhs);
    }

private:
    WallpaperKind m_kind = WallpaperKind::Image;
    WallpaperSourceKind m_sourceKind = WallpaperSourceKind::ExternalFile;
    WallpaperOrigin m_origin = WallpaperOrigin::User;
    WallpaperFit m_fit = WallpaperFit::Cover;
    WallpaperScope m_scope = WallpaperScope::Global;
    QString m_logicalId;
    QString m_source;
    QString m_resolvedSource;
    QString m_displayName;
};

} // namespace Paper

Q_DECLARE_METATYPE(Paper::WallpaperDescriptor)
