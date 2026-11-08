#pragma once

// Runtime macOS Tahoe app tiles (see shared/tiles, Rust). The core library
// only knows this hook: apps that link astrea-shared-app-tiles install the
// renderer at startup, and without one the icon provider presents artwork as
// it is.

#include <QColor>
#include <QImage>
#include <QString>

namespace AstreaTiles {

enum class Variant { Light, Dark };

struct Masters final {
    QImage light;   // kMasterExtent² straight RGBA8888
    QImage dark;
    bool isNull() const { return light.isNull() || dark.isNull(); }
    const QImage &of(Variant v) const { return v == Variant::Light ? light : dark; }
};

// macOS Tahoe icon styles; Default presents icons as they are.
enum class Style { Default = 0, Dark = 1, Clear = 2, Tinted = 3 };

struct StyleRequest final {
    Style style = Style::Default;
    QRgb tint = 0x0a84ff;   // Tinted only
};

struct Renderer final {
    // Builds both 512 px masters from straight-alpha artwork; null on failure.
    // Must be safe to call from worker threads.
    Masters (*render)(const QImage &artwork, bool keepDark) = nullptr;
    // Restyles a finished masterExtent² app icon (straight RGBA8888); `dark`
    // picks the style's dark-mode form, `tint` is used by Tinted. Null on
    // failure. Must be safe to call from worker threads.
    QImage (*style)(const QImage &icon, Style style, bool dark, QRgb tint) = nullptr;
    int masterExtent = 512;
    int version = 0;
    int styleVersion = 0;
};

void setRenderer(const Renderer &renderer);
Renderer renderer();
bool available();

// Decodes source art for tiling at master resolution. SVGs are rasterised to
// fit the master; raster art keeps its own size. Thread-safe (no QIcon).
QImage loadArtwork(const QString &path, int masterExtent);

// `artwork` centred on a transparent extent² canvas, scaled to fit it.
QImage fitToCanvas(const QImage &artwork, int extent);

Style styleFromString(const QString &value);

// Area-averaged downscale of a master to `extent` px, premultiplied output.
QImage scaleMaster(const QImage &master, int extent);

} // namespace AstreaTiles
