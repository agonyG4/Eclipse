#pragma once

#include <QJsonArray>
#include <QString>

class SpotlightBackend {
public:
    virtual ~SpotlightBackend() = default;

    virtual bool create(const QString &astreaRoot, const QString &locale,
                        QString *errorOut = nullptr) = 0;
    virtual bool createWithCatalog(const QString &astreaRoot, const QString &locale,
                                   const QJsonArray &catalog,
                                   QString *errorOut = nullptr) = 0;
    virtual bool reload(QString *errorOut = nullptr) = 0;
    virtual bool setCatalog(const QJsonArray &catalog,
                            QString *errorOut = nullptr) = 0;
    virtual QJsonArray search(const QString &query, int limit,
                              QString *errorOut = nullptr) = 0;
    virtual bool recordLaunch(const QString &desktopId,
                              QString *errorOut = nullptr) = 0;
    virtual bool ensureConfig(QString *errorOut = nullptr) = 0;
    virtual QJsonArray watchedDirectories() = 0;
};
