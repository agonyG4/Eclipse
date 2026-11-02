#pragma once
#include <QByteArray>
#include <QSize>
#include <QString>

namespace Astrea::SystemFontConfig {
// Update only the owned block in the user's fonts.conf; an empty pair removes it.
bool apply(const QString &path, const QString &primary, const QString &mono, QString *error);

struct OutputDensity {
    QString name;
    QSize pixels;
    QSize physicalMm;
    double scale = 1.0;
};
// Logical dpi follows the 96 dpi convention times the output scale; physical size is diagnostic.
double fontconfigDpi(const OutputDensity &output);
// Diagonal pixels per inch, or 0 when the physical size is unknown.
double physicalPpi(const OutputDensity &output);
// EDID base block bytes 21 and 22 hold the image size in cm; an invalid block yields an empty size.
QSize edidPhysicalSizeMm(const QByteArray &edid);
// Own the dpi rule: rewrite the display block and drop hand-set dpi edits elsewhere.
bool applyDpi(const QString &path, const OutputDensity &output, QString *error);
}
