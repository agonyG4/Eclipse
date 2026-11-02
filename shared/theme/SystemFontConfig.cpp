#include "SystemFontConfig.hpp"
#include "Typography.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QXmlStreamWriter>
#include <algorithm>
#include <cmath>

namespace Astrea::SystemFontConfig {
namespace {
const QString typographyBegin = QStringLiteral("<!-- Astrea typography begin -->");
const QString typographyEnd = QStringLiteral("<!-- Astrea typography end -->");
const QString displayBegin = QStringLiteral("<!-- Astrea display begin -->");
const QString displayEnd = QStringLiteral("<!-- Astrea display end -->");
bool validFamily(const QString &family) {
    return Typography::validatedPreferences({{"fontFamily", family}}).contains("fontFamily");
}
// Replace the block between begin and end with rules; empty rules remove it.
bool rewriteBlock(const QString &path, const QString &begin, const QString &end, const QString &rules,
                  bool takeOverDpi, QString *error)
{
    auto fail = [&](const QString &message) { if (error) *error = message; return false; };
    const bool remove = rules.isEmpty();
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return fail(QStringLiteral("Cannot create the Fontconfig directory."));
    QLockFile lock(path + QStringLiteral(".astrea.lock"));
    if (!lock.tryLock(1000)) return fail(QStringLiteral("Fontconfig is being updated. Try again."));
    if (QFileInfo(path).isSymLink())
        return fail(QStringLiteral("fonts.conf is a symbolic link; update its target manually."));
    QFile file(path);
    const bool existed = file.exists();
    if (!existed && remove) return true;
    if (existed && !file.open(QIODevice::ReadOnly)) return fail(file.errorString());
    const QByteArray original = existed ? file.readAll() : QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE fontconfig SYSTEM \"fonts.dtd\">\n<fontconfig>\n</fontconfig>\n");
    file.close();
    QString document = QString::fromUtf8(original);
    if (document.toUtf8() != original) return fail(QStringLiteral("fonts.conf must use UTF-8."));
    auto closingOffset = [](const QString &xml) -> qsizetype {
        QXmlStreamReader reader(xml);
        int depth = 0;
        qsizetype closing = -1;
        while (!reader.atEnd()) {
            reader.readNext();
            if (reader.isStartElement()) {
                if (depth == 0 && reader.name() != QStringLiteral("fontconfig")) return -1;
                ++depth;
            } else if (reader.isEndElement()) {
                if (--depth == 0) closing = reader.characterOffset() - QStringLiteral("</fontconfig>").size();
            }
        }
        if (reader.hasError() || closing < 0 || xml.mid(closing, 13) != QStringLiteral("</fontconfig>")) return -1;
        return closing;
    };
    if (closingOffset(document) < 0) return fail(QStringLiteral("Invalid fonts.conf; the original was preserved."));
    const qsizetype first = document.indexOf(begin);
    const qsizetype last = document.indexOf(end);
    if (first >= 0 || last >= 0) {
        if (first < 0 || last < first || document.indexOf(begin, first + begin.size()) >= 0
            || document.indexOf(end, last + end.size()) >= 0)
            return fail(QStringLiteral("Invalid Astrea Fontconfig block; the original was preserved."));
        document.remove(first, last + end.size() - first);
    }
    if (takeOverDpi && !remove) {
        // A hand-set dpi edit elsewhere would contradict the owned value.
        static const QRegularExpression dpiEdit(
            QStringLiteral(R"(\n?[ \t]*<edit\b[^>]*\bname\s*=\s*["']dpi["'][^>]*>.*?</edit>)"),
            QRegularExpression::DotMatchesEverythingOption);
        document.remove(dpiEdit);
    }
    if (!remove) {
        const auto closing = closingOffset(document);
        if (closing < 0) return fail(QStringLiteral("Invalid fonts.conf after removing the owned block."));
        document.insert(closing, rules);
    }
    if (closingOffset(document) < 0) return fail(QStringLiteral("Invalid updated fonts.conf; the original was preserved."));
    const QByteArray next = document.toUtf8();
    if (next == original) return true;
    // Keep the first original file as an additional recovery copy.
    const QString backup = path + QStringLiteral(".before-astrea");
    if (existed && !QFile::exists(backup) && !QFile::copy(path, backup))
        return fail(QStringLiteral("Cannot back up fonts.conf; the original was preserved."));
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) return fail(output.errorString());
    if (output.write(next) != next.size() || !output.commit()) return fail(output.errorString());
    return true;
}
}

bool apply(const QString &path, const QString &primary, const QString &mono, QString *error)
{
    if (error) error->clear();
    QString rules;
    if (!primary.isEmpty() || !mono.isEmpty()) {
        if (!validFamily(primary) || !validFamily(mono)) {
            if (error) *error = QStringLiteral("Invalid font family.");
            return false;
        }
        QXmlStreamWriter xml(&rules);
        xml.writeComment(QStringLiteral(" Astrea typography begin "));
        for (const auto &generic : {"sans", "sans-serif", "system-ui", "ui-sans-serif", "monospace", "ui-monospace"}) {
            xml.writeStartElement("alias"); xml.writeAttribute("binding", "strong");
            xml.writeTextElement("family", QLatin1String(generic));
            xml.writeStartElement("prefer");
            xml.writeTextElement("family", QString::fromLatin1(generic).contains("mono") ? mono : primary);
            xml.writeEndElement(); xml.writeEndElement();
        }
        xml.writeComment(QStringLiteral(" Astrea typography end "));
    }
    return rewriteBlock(path, typographyBegin, typographyEnd, rules, false, error);
}

double fontconfigDpi(const OutputDensity &output)
{
    if (!std::isfinite(output.scale) || output.scale <= 0) return 96.0;
    // Fractional scales arrive as 120ths; two decimals keep the file stable across tiny drifts.
    return std::round(96.0 * std::clamp(output.scale, 0.5, 4.0) * 100.0) / 100.0;
}

double physicalPpi(const OutputDensity &output)
{
    if (output.pixels.isEmpty() || output.physicalMm.isEmpty()) return 0;
    const double inches = std::hypot(output.physicalMm.width(), output.physicalMm.height()) / 25.4;
    return std::hypot(output.pixels.width(), output.pixels.height()) / inches;
}

QSize edidPhysicalSizeMm(const QByteArray &edid)
{
    static const QByteArray header("\x00\xff\xff\xff\xff\xff\xff\x00", 8);
    if (edid.size() < 128 || !edid.startsWith(header)) return {};
    quint8 sum = 0;
    for (int i = 0; i < 128; ++i) sum += quint8(edid.at(i));
    // Zero means unknown or a projector (aspect ratio only).
    const int widthCm = quint8(edid.at(21)), heightCm = quint8(edid.at(22));
    if (sum != 0 || widthCm == 0 || heightCm == 0) return {};
    return {widthCm * 10, heightCm * 10};
}

bool applyDpi(const QString &path, const OutputDensity &output, QString *error)
{
    if (error) error->clear();
    const double dpi = fontconfigDpi(output);
    const QString value = QString::number(dpi, 'g', 6);
    QString rules;
    QXmlStreamWriter xml(&rules);
    xml.writeComment(QStringLiteral(" Astrea display begin "));
    const double ppi = physicalPpi(output);
    QString detail = QStringLiteral(" %1: %2x%3 px, scale %4")
        .arg(output.name.isEmpty() ? QStringLiteral("output") : output.name)
        .arg(output.pixels.width()).arg(output.pixels.height()).arg(output.scale, 0, 'g', 4);
    if (ppi > 0)
        detail += QStringLiteral(", %1x%2 mm, %3 ppi physical")
            .arg(output.physicalMm.width()).arg(output.physicalMm.height()).arg(ppi, 0, 'f', 1);
    // Output names come from the compositor; keep the comment well formed.
    xml.writeComment(detail.replace(QStringLiteral("--"), QStringLiteral("- ")) + QLatin1Char(' '));
    for (const auto &target : {"pattern", "font"}) {
        xml.writeStartElement("match"); xml.writeAttribute("target", QLatin1String(target));
        xml.writeStartElement("edit"); xml.writeAttribute("name", "dpi"); xml.writeAttribute("mode", "assign");
        xml.writeTextElement("double", value);
        xml.writeEndElement(); xml.writeEndElement();
    }
    xml.writeComment(QStringLiteral(" Astrea display end "));
    return rewriteBlock(path, displayBegin, displayEnd, rules, true, error);
}
}
