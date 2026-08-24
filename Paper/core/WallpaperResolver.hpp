#pragma once

#include "WallpaperDescriptor.hpp"

#include <QString>

namespace Paper {

enum class WallpaperResolutionError {
    None,
    InvalidDescriptor,
    InvalidUri,
    UnsupportedKind,
    UnsupportedScope,
    SourceMissing,
    SourceNotRegularFile,
    SourceUnreadable,
    UnsupportedImage,
    FactoryDefaultUnavailable,
    EmergencyFallback,
};

struct WallpaperResolution final
{
    WallpaperDescriptor descriptor;
    WallpaperResolutionError error = WallpaperResolutionError::None;
    QString message;

    bool ok() const
    {
        return error == WallpaperResolutionError::None
            || error == WallpaperResolutionError::EmergencyFallback;
    }
};

class WallpaperResolver final
{
public:
    explicit WallpaperResolver(QString factoryDefaultSource = {},
                                QString emergencySource = QStringLiteral(":/qt/qml/Astrea/Paper/assets/emergency.svg"));

    WallpaperResolution resolve(const WallpaperDescriptor &candidate) const;
    WallpaperResolution factoryDefault(WallpaperFit fit = WallpaperFit::Cover) const;

    static QString expandSource(const QString &source);

private:
    WallpaperResolution resolveLocal(const WallpaperDescriptor &candidate,
                                      const QString &path) const;
    QStringList factoryCandidates() const;

    QString m_factoryDefaultSource;
    QString m_emergencySource;
};

} // namespace Paper

Q_DECLARE_METATYPE(Paper::WallpaperResolutionError)
