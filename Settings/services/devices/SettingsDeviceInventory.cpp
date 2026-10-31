#include "services/devices/SettingsDeviceInventory.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStorageInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>

namespace {

QString readAttribute(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.read(256)).trimmed();
}

QString safeText(const QString &text)
{
    QString cleaned;
    cleaned.reserve(qMin(text.size(), 128));
    for (const QChar c : text.left(128)) {
        if (c.isPrint() && c != QChar::Null)
            cleaned.append(c);
    }
    return cleaned.trimmed();
}

bool hasUsbStorageInterface(const QString &sysRoot, const QString &usbName)
{
    bool hasMassStorageInterface = false;
    const QDir devices(sysRoot + QStringLiteral("/bus/usb/devices"));
    for (const QString &entry : devices.entryList(QDir::Dirs | QDir::System | QDir::NoDotAndDotDot)) {
        if (!entry.startsWith(usbName + QLatin1Char(':')))
            continue;
        if (readAttribute(devices.filePath(entry + QStringLiteral("/bInterfaceClass")))
                .compare(QStringLiteral("08"), Qt::CaseInsensitive) == 0) {
            hasMassStorageInterface = true;
            break;
        }
    }
    if (!hasMassStorageInterface)
        return false;

    // Show a USB mass-storage peripheral in the Storage section only once a
    // corresponding block device exists. Otherwise it remains visible as USB.
    const QDir blocks(sysRoot + QStringLiteral("/class/block"));
    const QString parentToken = QLatin1Char('/') + usbName + QLatin1Char('/');
    for (const QString &block : blocks.entryList(QDir::Dirs | QDir::System | QDir::NoDotAndDotDot)) {
        const QString path = QFileInfo(blocks.filePath(block)).canonicalFilePath();
        if (path.contains(parentToken))
            return true;
    }
    return false;
}

QString usbCategory(const QString &sysRoot, const QString &name, const QString &product)
{
    const QString lower = product.toLower();
    if (lower.contains(QStringLiteral("keyboard")) || lower.contains(QStringLiteral("keychron")))
        return QStringLiteral("keyboard");
    if (lower.contains(QStringLiteral("mouse")) || lower.contains(QStringLiteral("trackball")))
        return QStringLiteral("mouse");
    if (lower.contains(QStringLiteral("gamepad")) || lower.contains(QStringLiteral("controller"))
        || lower.contains(QStringLiteral("joystick")))
        return QStringLiteral("gamepad");
    if (lower.contains(QStringLiteral("camera")) || lower.contains(QStringLiteral("webcam")))
        return QStringLiteral("camera");
    if (lower.contains(QStringLiteral("headset")) || lower.contains(QStringLiteral("headphone")))
        return QStringLiteral("headphones");
    if (lower.contains(QStringLiteral("speaker")) || lower.contains(QStringLiteral("audio")))
        return QStringLiteral("audio");
    if (lower.contains(QStringLiteral("printer")))
        return QStringLiteral("printer");

    const QDir dir(sysRoot + QStringLiteral("/bus/usb/devices"));
    for (const QString &entry : dir.entryList(QDir::Dirs | QDir::System | QDir::NoDotAndDotDot)) {
        if (!entry.startsWith(name + QLatin1Char(':')))
            continue;
        const QString path = dir.filePath(entry);
        if (readAttribute(path + QStringLiteral("/bInterfaceClass")) != QStringLiteral("03"))
            continue;
        const QString proto = readAttribute(path + QStringLiteral("/bInterfaceProtocol"));
        if (proto == QStringLiteral("01"))
            return QStringLiteral("keyboard");
        if (proto == QStringLiteral("02"))
            return QStringLiteral("mouse");
    }
    return QStringLiteral("usb");
}

QVariantMap makeDevice(const QString &id, const QString &name, const QString &detail,
                       const QString &section, const QString &icon)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("detail"), detail},
        {QStringLiteral("section"), section},
        {QStringLiteral("iconKey"), icon},
    };
}

QString sectionForIcon(const QString &icon)
{
    return icon == QStringLiteral("keyboard") || icon == QStringLiteral("mouse")
                   || icon == QStringLiteral("gamepad")
        ? QStringLiteral("input") : QStringLiteral("usb");
}

void collectUsb(const QString &sysRoot, QVariantList &result)
{
    const QDir devices(sysRoot + QStringLiteral("/bus/usb/devices"));
    for (const QString &entry : devices.entryList(QDir::Dirs | QDir::System | QDir::NoDotAndDotDot,
                                                    QDir::Name)) {
        const QString path = devices.filePath(entry);
        const QString vendor = readAttribute(path + QStringLiteral("/idVendor"));
        const QString productId = readAttribute(path + QStringLiteral("/idProduct"));
        if (vendor.isEmpty() || productId.isEmpty())
            continue;
        // USB host root hubs and external hubs are not user accessories.
        if (readAttribute(path + QStringLiteral("/bDeviceClass")) == QStringLiteral("09"))
            continue;
        const QString model = safeText(readAttribute(path + QStringLiteral("/product")));
        const QString maker = safeText(readAttribute(path + QStringLiteral("/manufacturer")));
        const QString label = !model.isEmpty() ? model
                             : QStringLiteral("USB %1:%2").arg(vendor, productId);
        // Mass storage is represented by the block device below, not twice.
        if (hasUsbStorageInterface(sysRoot, entry))
            continue;
        const QString icon = usbCategory(sysRoot, entry, model);
        const QString detail = maker.isEmpty() || maker == model
            ? QStringLiteral("USB · %1:%2").arg(vendor, productId)
            : QStringLiteral("%1 · USB").arg(maker);
        // Physical port + IDs distinguishes identical devices plugged into separate ports.
        const QString stableId = QStringLiteral("usb:%1:%2:%3").arg(entry, vendor, productId);
        result.append(makeDevice(stableId, label, detail, sectionForIcon(icon), icon));
    }
}

bool isUsbBacked(const QString &canonical)
{
    // sysfs USB paths contain a /usbN/ bus component.
    return canonical.contains(QRegularExpression(QStringLiteral("/usb[0-9]+/")));
}

QString formatBytes(qulonglong bytes)
{
    // Decimal units, as drive makers and Finder label capacity.
    const double gb = static_cast<double>(bytes) / 1000000000.0;
    if (gb >= 1000.0) {
        const double tb = gb / 1000.0;
        return QString::number(tb, 'f', tb < 10 ? 1 : 0).replace(QStringLiteral(".0"), QString())
            + QStringLiteral(" TB");
    }
    return QString::number(gb, 'f', gb < 10 ? 1 : 0) + QStringLiteral(" GB");
}

// Used and total bytes across the mounted filesystems that live on one disk.
// Btrfs subvolumes mount the same device several times, so each device counts once.
QPair<qulonglong, qulonglong> mountedUsage(const QString &diskName)
{
    const QString prefix = QStringLiteral("/dev/") + diskName;
    QSet<QString> seen;
    qulonglong used = 0;
    qulonglong total = 0;
    for (const QStorageInfo &volume : QStorageInfo::mountedVolumes()) {
        const QString device = QString::fromLocal8Bit(volume.device());
        if (!volume.isValid() || !volume.isReady() || !device.startsWith(prefix) || seen.contains(device))
            continue;
        seen.insert(device);
        total += static_cast<qulonglong>(volume.bytesTotal());
        used += static_cast<qulonglong>(volume.bytesTotal() - volume.bytesFree());
    }
    return {used, total};
}

void collectStorage(const QString &sysRoot, QVariantList &result)
{
    const QDir blocks(sysRoot + QStringLiteral("/class/block"));
    for (const QString &name : blocks.entryList(QDir::Dirs | QDir::System | QDir::NoDotAndDotDot,
                                                 QDir::Name)) {
        if (name.startsWith(QStringLiteral("loop")) || name.startsWith(QStringLiteral("ram"))
            || name.startsWith(QStringLiteral("zram")) || name.startsWith(QStringLiteral("dm-"))
            || name.startsWith(QStringLiteral("md")) || name.startsWith(QStringLiteral("sr")))
            continue;
        const QString path = blocks.filePath(name);
        if (QFileInfo::exists(path + QStringLiteral("/partition")))
            continue;
        const QString sectorsText = readAttribute(path + QStringLiteral("/size"));
        bool ok = false;
        const qulonglong sectors = sectorsText.toULongLong(&ok);
        if (!ok || sectors == 0)
            continue;
        const QString model = safeText(readAttribute(path + QStringLiteral("/device/model")));
        const bool removable = readAttribute(path + QStringLiteral("/removable")) == QStringLiteral("1");
        const bool usb = isUsbBacked(QFileInfo(path).canonicalFilePath());
        const QString label = model.isEmpty() ? name : model;
        const qulonglong bytes = sectors * 512ULL;
        // Mounts describe the running system, so a test sysfs root never reads them.
        const auto [usedBytes, mountedBytes] = sysRoot == QStringLiteral("/sys")
            ? mountedUsage(name) : QPair<qulonglong, qulonglong>{0, 0};
        QString detail = QStringLiteral("%1 · %2 · %3")
            .arg(removable ? QStringLiteral("Removable") : usb ? QStringLiteral("USB") : QStringLiteral("Internal"),
                 formatBytes(bytes), name);
        if (mountedBytes > 0)
            detail += QStringLiteral(" · %1 free").arg(formatBytes(mountedBytes - usedBytes));
        // Prefer a hardware identifier over kernel enumeration names (sda/sdb).
        // Kernel names may change when another flash drive is plugged in first.
        QString identity = readAttribute(path + QStringLiteral("/device/wwid"));
        if (identity.isEmpty())
            identity = readAttribute(path + QStringLiteral("/device/serial"));
        if (identity.isEmpty())
            identity = readAttribute(path + QStringLiteral("/wwid"));
        identity = safeText(identity);
        if (identity.isEmpty() && usb) {
            const QRegularExpression portPattern(QStringLiteral("/([0-9]+-[0-9.]+)(?:/|:)"));
            const auto match = portPattern.match(QFileInfo(path).canonicalFilePath());
            if (match.hasMatch())
                identity = QStringLiteral("port:") + match.captured(1);
        }
        const QString id = QStringLiteral("block:") + (identity.isEmpty() ? name : identity);
        QVariantMap device = makeDevice(id, label, detail, QStringLiteral("storage"),
                                        removable || usb ? QStringLiteral("usb") : QStringLiteral("hard-drive"));
        if (mountedBytes > 0)
            device.insert(QStringLiteral("usage"), static_cast<double>(usedBytes) / static_cast<double>(mountedBytes));
        result.append(device);
    }
}

void collectPlatformInput(const QString &sysRoot, QVariantList &result)
{
    QSet<QString> seen;
    const QDir inputs(sysRoot + QStringLiteral("/class/input"));
    for (const QString &entry : inputs.entryList({QStringLiteral("event*")},
                                                 QDir::Dirs | QDir::System | QDir::NoDotAndDotDot,
                                                 QDir::Name)) {
        const QString path = inputs.filePath(entry + QStringLiteral("/device"));
        const QString canonical = QFileInfo(path).canonicalFilePath();
        if (isUsbBacked(canonical) || canonical.contains(QStringLiteral("bluetooth"),
                                                          Qt::CaseInsensitive))
            continue; // Already represented by USB/Bluetooth.
        const QString label = safeText(readAttribute(path + QStringLiteral("/name")));
        if (label.isEmpty())
            continue;
        const QString lower = label.toLower();
        QString icon;
        if (lower.contains(QStringLiteral("keyboard")) || lower.contains(QStringLiteral("at translated")))
            icon = QStringLiteral("keyboard");
        else if (lower.contains(QStringLiteral("mouse")) || lower.contains(QStringLiteral("touchpad"))
                 || lower.contains(QStringLiteral("trackpoint")))
            icon = QStringLiteral("mouse");
        else if (lower.contains(QStringLiteral("joystick")) || lower.contains(QStringLiteral("gamepad"))
                 || lower.contains(QStringLiteral("controller")))
            icon = QStringLiteral("gamepad");
        else
            continue;
        const QString identifier = canonical.isEmpty() ? path : canonical;
        if (seen.contains(identifier))
            continue;
        seen.insert(identifier);
        result.append(makeDevice(QStringLiteral("input:") + identifier, label,
                                 QStringLiteral("Built-in / platform input"),
                                 QStringLiteral("input"), icon));
    }
}

} // namespace

QVariantList SettingsDeviceInventory::enumerate(const QString &sysRoot)
{
    QVariantList devices;
    collectUsb(sysRoot, devices);
    collectStorage(sysRoot, devices);
    collectPlatformInput(sysRoot, devices);
    std::sort(devices.begin(), devices.end(), [](const QVariant &lhs, const QVariant &rhs) {
        const auto left = lhs.toMap();
        const auto right = rhs.toMap();
        if (left.value(QStringLiteral("section")) != right.value(QStringLiteral("section")))
            return left.value(QStringLiteral("section")).toString()
                < right.value(QStringLiteral("section")).toString();
        return left.value(QStringLiteral("name")).toString().compare(
                   right.value(QStringLiteral("name")).toString(), Qt::CaseInsensitive) < 0;
    });
    return devices;
}
