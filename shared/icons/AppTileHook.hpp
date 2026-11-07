#pragma once

// Runtime macOS Tahoe app tiles (see shared/tiles, a Rust port of
// MacOS-Like's tile pass). The core library only knows this hook: apps that
// link astrea-shared-app-tiles install the renderer at startup, and without one
// the icon provider presents artwork as it is.

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

struct Renderer final {
    // Builds both 512 px masters from straight-alpha artwork; null on failure.
    // Must be safe to call from worker threads.
    Masters (*render)(const QImage &artwork, bool keepDark) = nullptr;
    int masterExtent = 512;
    int version = 0;
};

void setRenderer(const Renderer &renderer);
Renderer renderer();
bool available();

// Decodes source art for tiling at master resolution. SVGs are rasterised to
// fit the master; raster art keeps its own size. Thread-safe (no QIcon).
QImage loadArtwork(const QString &path, int masterExtent);

// Area-averaged downscale of a master to `extent` px, premultiplied output.
QImage scaleMaster(const QImage &master, int extent);

} // namespace AstreaTiles
