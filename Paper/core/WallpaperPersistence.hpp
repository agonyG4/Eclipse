#pragma once

#include "WallpaperDescriptor.hpp"

#include <QString>

#include <optional>

namespace Paper {

struct WallpaperSelection final
{
    QString wallpaperId;
    WallpaperFit fit = WallpaperFit::Cover;

    bool isValid() const { return !wallpaperId.trimmed().isEmpty(); }
};

class WallpaperPersistence
{
public:
    virtual ~WallpaperPersistence() = default;

    virtual std::optional<WallpaperDescriptor> load(QString *errorMessage = nullptr) const = 0;
    virtual bool save(const WallpaperDescriptor &descriptor,
                      QString *errorMessage = nullptr) = 0;
    virtual bool clear(QString *errorMessage = nullptr) = 0;
    virtual QString location() const = 0;

    virtual std::optional<WallpaperSelection> loadSelection(
        QString *errorMessage = nullptr) const;
    virtual bool saveSelection(const WallpaperSelection &selection,
                               QString *errorMessage = nullptr);
};

class XdgWallpaperPersistence final : public WallpaperPersistence
{
public:
    explicit XdgWallpaperPersistence(QString path = {}, QString legacyPath = {});

    std::optional<WallpaperDescriptor> load(QString *errorMessage = nullptr) const override;
    bool save(const WallpaperDescriptor &descriptor,
              QString *errorMessage = nullptr) override;
    bool clear(QString *errorMessage = nullptr) override;
    QString location() const override;
    std::optional<WallpaperSelection> loadSelection(
        QString *errorMessage = nullptr) const override;
    bool saveSelection(const WallpaperSelection &selection,
                       QString *errorMessage = nullptr) override;
    std::optional<WallpaperDescriptor> migrateLegacy() const;

private:
    static QString defaultPath();
    QString m_path;
    QString m_legacyPath;
};

} // namespace Paper

Q_DECLARE_METATYPE(Paper::WallpaperSelection)
