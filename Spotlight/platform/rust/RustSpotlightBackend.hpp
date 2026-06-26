#pragma once

#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <memory>

struct AstreaSpotlightBackend;

class RustSpotlightBackend {
public:
    RustSpotlightBackend();
    ~RustSpotlightBackend();

    bool create(const QString &astreaRoot, const QString &locale, QString *errorOut = nullptr);
    void destroy();
    bool reload(QString *errorOut = nullptr);
    QJsonArray search(const QString &query, int limit, QString *errorOut = nullptr);
    bool recordLaunch(const QString &desktopId, QString *errorOut = nullptr);
    bool ensureConfig(QString *errorOut = nullptr);
    QJsonArray watchedDirectories();
    bool isValid() const { return m_backend != nullptr; }

private:
    AstreaSpotlightBackend *m_backend = nullptr;
    QString m_astreaRoot;
    QString m_locale;
};
