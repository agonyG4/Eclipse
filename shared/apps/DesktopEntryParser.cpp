#include "apps/DesktopEntryParser.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

bool parseBoolean(const QString &value)
{
    return value.trimmed().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
}

QString decodeString(const QString &value)
{
    QString result;
    result.reserve(value.size());
    for (int index = 0; index < value.size(); ++index) {
        const QChar character = value.at(index);
        if (character != QLatin1Char('\\')) {
            result.append(character);
            continue;
        }

        if (index + 1 >= value.size()) {
            result.append(QLatin1Char('\\'));
            continue;
        }

        const QChar escaped = value.at(++index);
        if (escaped == QLatin1Char('s'))
            result.append(QLatin1Char(' '));
        else if (escaped == QLatin1Char('n'))
            result.append(QLatin1Char('\n'));
        else if (escaped == QLatin1Char('t'))
            result.append(QLatin1Char('\t'));
        else if (escaped == QLatin1Char('r'))
            result.append(QLatin1Char('\r'));
        else if (escaped == QLatin1Char('\\'))
            result.append(QLatin1Char('\\'));
        else {
            result.append(QLatin1Char('\\'));
            result.append(escaped);
        }
    }
    return result;
}

QStringList decodeList(const QString &value)
{
    QStringList result;
    QString current;
    current.reserve(value.size());

    for (int index = 0; index < value.size(); ++index) {
        const QChar character = value.at(index);
        if (character == QLatin1Char('\\')) {
            if (index + 1 >= value.size()) {
                current.append(QLatin1Char('\\'));
                continue;
            }

            const QChar escaped = value.at(++index);
            if (escaped == QLatin1Char(';'))
                current.append(QLatin1Char(';'));
            else if (escaped == QLatin1Char('s'))
                current.append(QLatin1Char(' '));
            else if (escaped == QLatin1Char('n'))
                current.append(QLatin1Char('\n'));
            else if (escaped == QLatin1Char('t'))
                current.append(QLatin1Char('\t'));
            else if (escaped == QLatin1Char('r'))
                current.append(QLatin1Char('\r'));
            else if (escaped == QLatin1Char('\\'))
                current.append(QLatin1Char('\\'));
            else {
                current.append(QLatin1Char('\\'));
                current.append(escaped);
            }
        } else if (character == QLatin1Char(';')) {
            const QString item = current.trimmed();
            if (!item.isEmpty())
                result.append(item);
            current.clear();
        } else {
            current.append(character);
        }
    }

    const QString item = current.trimmed();
    if (!item.isEmpty())
        result.append(item);
    return result;
}

QString localeForKey(const QString &key, const QString &base)
{
    const QString prefix = base + QLatin1Char('[');
    if (!key.startsWith(prefix) || !key.endsWith(QLatin1Char(']')))
        return {};
    return key.mid(prefix.size(), key.size() - prefix.size() - 1);
}

void assignStringField(DesktopEntryRecord &record, const QString &key, const QString &value)
{
    if (key == QStringLiteral("Name")) {
        record.name = value;
    } else if (key == QStringLiteral("GenericName")) {
        record.genericName = value;
    } else if (key == QStringLiteral("Comment")) {
        record.comment = value;
    } else if (key == QStringLiteral("Icon")) {
        record.icon = value;
    } else if (key == QStringLiteral("Exec")) {
        record.exec = value;
    } else if (key == QStringLiteral("TryExec")) {
        record.tryExec = value;
    } else if (key == QStringLiteral("StartupWMClass")) {
        record.startupWmClass = value;
    }
}

} // namespace

namespace DesktopEntryParser {

std::optional<DesktopEntryRecord> parse(const QString &sourceFilePath,
                                         const QString &applicationRoot)
{
    QFile file(sourceFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return std::nullopt;

    const QString absoluteSource = QFileInfo(sourceFilePath).absoluteFilePath();
    const QString absoluteRoot = QFileInfo(applicationRoot).absoluteFilePath();
    QString relativePath = QDir(absoluteRoot).relativeFilePath(absoluteSource);
    relativePath = QDir::cleanPath(relativePath);
    if (relativePath.isEmpty() || relativePath == QStringLiteral("."))
        return std::nullopt;

    const QString desktopSuffix = QStringLiteral(".desktop");
    if (!relativePath.endsWith(desktopSuffix))
        return std::nullopt;

    DesktopEntryRecord record;
    record.sourceFilePath = absoluteSource;
    record.desktopFileName = relativePath;
    record.desktopFileName.replace(QLatin1Char('/'), QLatin1Char('-'));
    record.id = record.desktopFileName.left(record.desktopFileName.size() - desktopSuffix.size());

    bool inDesktopEntry = false;
    bool typeSeen = false;
    const QString text = QString::fromUtf8(file.readAll());
    for (const QString &rawLine : text.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            inDesktopEntry = line == QStringLiteral("[Desktop Entry]");
            continue;
        }
        if (!inDesktopEntry || line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        const int separator = line.indexOf(QLatin1Char('='));
        if (separator <= 0)
            continue;

        const QString key = line.left(separator).trimmed();
        const QString rawValue = line.mid(separator + 1).trimmed();
        if (key == QStringLiteral("Type")) {
            typeSeen = true;
            if (decodeString(rawValue) != QStringLiteral("Application"))
                return std::nullopt;
            continue;
        }

        if (key == QStringLiteral("Keywords")) {
            record.keywords = decodeList(rawValue);
        } else if (key == QStringLiteral("Categories")) {
            record.categories = decodeList(rawValue);
        } else if (key == QStringLiteral("OnlyShowIn")) {
            record.onlyShowIn = decodeList(rawValue);
        } else if (key == QStringLiteral("NotShowIn")) {
            record.notShowIn = decodeList(rawValue);
        } else if (key == QStringLiteral("Terminal")) {
            record.terminal = parseBoolean(rawValue);
        } else if (key == QStringLiteral("NoDisplay")) {
            record.noDisplay = parseBoolean(rawValue);
        } else if (key == QStringLiteral("Hidden")) {
            record.hidden = parseBoolean(rawValue);
        } else if (key == QStringLiteral("Name") || key == QStringLiteral("GenericName")
                   || key == QStringLiteral("Comment") || key == QStringLiteral("Icon")
                   || key == QStringLiteral("Exec") || key == QStringLiteral("TryExec")
                   || key == QStringLiteral("StartupWMClass")) {
            assignStringField(record, key, decodeString(rawValue));
        } else {
            const QString nameLocale = localeForKey(key, QStringLiteral("Name"));
            const QString genericLocale = localeForKey(key, QStringLiteral("GenericName"));
            const QString commentLocale = localeForKey(key, QStringLiteral("Comment"));
            const QString keywordsLocale = localeForKey(key, QStringLiteral("Keywords"));
            if (!nameLocale.isNull())
                record.localizedNames.insert(nameLocale, decodeString(rawValue));
            else if (!genericLocale.isNull())
                record.localizedGenericNames.insert(genericLocale, decodeString(rawValue));
            else if (!commentLocale.isNull())
                record.localizedComments.insert(commentLocale, decodeString(rawValue));
            else if (!keywordsLocale.isNull())
                record.localizedKeywords.insert(keywordsLocale, decodeList(rawValue));
        }
    }

    if (!typeSeen || record.name.isEmpty())
        return std::nullopt;
    return record;
}

} // namespace DesktopEntryParser
