#include "icons/AppTileHook.hpp"

#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMutex>
#include <QMutexLocker>

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
