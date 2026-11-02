#include "SystemFontConfig.hpp"
#include "Typography.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace Astrea::SystemFontConfig {
namespace {
const QString begin = QStringLiteral("<!-- Astrea typography begin -->");
const QString end = QStringLiteral("<!-- Astrea typography end -->");
bool validFamily(const QString &family) {
    return Typography::validatedPreferences({{"fontFamily", family}}).contains("fontFamily");
}
}
bool apply(const QString &path, const QString &primary, const QString &mono, QString *error)
{
    if (error) error->clear();
    auto fail = [&](const QString &message) { if (error) *error = message; return false; };
    const bool remove = primary.isEmpty() && mono.isEmpty();
    if (!remove && (!validFamily(primary) || !validFamily(mono)))
        return fail(QStringLiteral("Invalid font family."));
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
    if (!remove) {
        QString rules;
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
