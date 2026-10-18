#pragma once

#include "core/SpotlightBackend.hpp"

#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <memory>

struct AstreaSpotlightBackend;

class RustSpotlightBackend : public SpotlightBackend {
public:
    RustSpotlightBackend();
    ~RustSpotlightBackend() override;

    bool create(const QString &astreaRoot, const QString &locale,
                QString *errorOut = nullptr) override;
    bool createWithCatalog(const QString &astreaRoot, const QString &locale,
                           const QJsonArray &catalog,
                           QString *errorOut = nullptr) override;
    void destroy();
    bool reload(QString *errorOut = nullptr) override;
    bool setCatalog(const QJsonArray &catalog,
                    QString *errorOut = nullptr) override;
    QJsonArray search(const QString &query, int limit,
                      QString *errorOut = nullptr) override;
    bool recordLaunch(const QString &desktopId,
                      QString *errorOut = nullptr) override;
    bool ensureConfig(QString *errorOut = nullptr) override;
    QJsonArray watchedDirectories() override;
    bool isValid() const { return m_backend != nullptr; }

private:
    AstreaSpotlightBackend *m_backend = nullptr;
    QString m_astreaRoot;
    QString m_locale;
};
