#pragma once

#include <QBuffer>
#include <QImage>
#include <QString>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

namespace Astrea::Typhon {

// Takes ownership even when the event is stale or the payload is invalid.
inline QString windowIconFromPixels(int fd, quint32 size)
{
    struct CloseFd {
        int value;
        ~CloseFd() { if (value >= 0) ::close(value); }
    } closeFd{fd};
    if (fd < 0 || size == 0 || size > 256)
        return {};
    const qint64 bytes = static_cast<qint64>(size) * size * 4;
    struct stat info{};
    if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size != bytes)
        return {};
    QImage image(static_cast<int>(size), static_cast<int>(size), QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        return {};
    qint64 offset = 0;
    while (offset < bytes) {
        const ssize_t count = ::pread(fd, image.bits() + offset, bytes - offset, offset);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return {};
        offset += count;
    }
    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
        return {};
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64());
}

} // namespace Astrea::Typhon
