#include "icons/AppTileHook.hpp"

#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>

namespace AstreaTiles {
namespace {

QMutex &rendererMutex()
{
    static QMutex mutex;
    return mutex;
}

Renderer &rendererSlot()
{
    static Renderer slot;
    return slot;
}

bool looksLikeSvg(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("svg") || suffix == QStringLiteral("svgz"))
        return true;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray head = file.read(512).trimmed().toLower();
    return head.startsWith('<') && head.contains("svg");
}

} // namespace

void setRenderer(const Renderer &renderer)
{
    QMutexLocker lock(&rendererMutex());
    rendererSlot() = renderer;
}

Renderer renderer()
{
    QMutexLocker lock(&rendererMutex());
    return rendererSlot();
}

bool available()
{
    return renderer().render != nullptr;
}

QImage loadArtwork(const QString &path, int masterExtent)
{
    if (looksLikeSvg(path)) {
        if (const auto rasterize = renderer().rasterizeSvg) {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                const QImage image = rasterize(file.readAll(), masterExtent);
                if (!image.isNull())
                    return image;
            }
        }
        QImageReader reader(path, "svg");
        QSize size = reader.size();
        if (!size.isValid() || size.isEmpty())
            size = QSize(masterExtent, masterExtent);
        size.scale(masterExtent, masterExtent, Qt::KeepAspectRatio);
        reader.setScaledSize(size);
        return reader.read().convertToFormat(QImage::Format_RGBA8888);
    }
    QImageReader reader(path);
    reader.setAutoTransform(true);
    return reader.read().convertToFormat(QImage::Format_RGBA8888);
}

QImage fitToCanvas(const QImage &artwork, int extent)
{
    if (artwork.isNull() || extent <= 0)
        return {};
    if (artwork.width() == extent && artwork.height() == extent)
        return artwork.convertToFormat(QImage::Format_RGBA8888);
    QImage canvas(extent, extent, QImage::Format_RGBA8888);
    canvas.fill(Qt::transparent);
    const QImage scaled = artwork.scaled(extent, extent, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation)
                              .convertToFormat(QImage::Format_RGBA8888);
    QPainter painter(&canvas);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage((extent - scaled.width()) / 2, (extent - scaled.height()) / 2, scaled);
    painter.end();
    return canvas;
}

Style styleFromString(const QString &value)
{
    if (value == QStringLiteral("dark"))
        return Style::Dark;
    if (value == QStringLiteral("clear"))
        return Style::Clear;
    if (value == QStringLiteral("tinted"))
        return Style::Tinted;
    return Style::Default;
}

QImage scaleMaster(const QImage &master, int extent)
{
    if (master.isNull() || extent <= 0)
        return {};
    QImage image = master.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    // Halve while that stays above twice the target, so a large ratio still
    // averages every source pixel; then one smooth pass to the exact size.
    while (image.width() / 2 >= extent * 2)
        image = image.scaled(image.width() / 2, image.height() / 2,
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (image.width() == extent)
        return image;
    return image.scaled(extent, extent, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace AstreaTiles
