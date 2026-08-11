#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

#include <optional>

struct DesktopEntryRecord {
    QString desktopFileName;
    QString id;
    QString name;
    QString genericName;
    QString comment;
    QHash<QString, QString> localizedNames;
    QHash<QString, QString> localizedGenericNames;
    QHash<QString, QString> localizedComments;
    QHash<QString, QStringList> localizedKeywords;
    QString icon;
    QString exec;
    QString tryExec;
    QString startupWmClass;
    QString sourceFilePath;
    QStringList keywords;
    QStringList categories;
    QStringList onlyShowIn;
    QStringList notShowIn;
    bool terminal = false;
    bool noDisplay = false;
    bool hidden = false;
};

namespace DesktopEntryParser {

std::optional<DesktopEntryRecord> parse(const QString &sourceFilePath,
                                         const QString &applicationRoot);

} // namespace DesktopEntryParser
