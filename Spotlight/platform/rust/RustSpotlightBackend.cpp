#include "platform/rust/RustSpotlightBackend.hpp"
#include "backend/include/astrea_spotlight_backend.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

RustSpotlightBackend::RustSpotlightBackend() = default;
RustSpotlightBackend::~RustSpotlightBackend() { destroy(); }

bool RustSpotlightBackend::create(const QString &astreaRoot, const QString &locale, QString *errorOut) {
    if (m_backend && m_astreaRoot == astreaRoot && m_locale == locale)
        return true;
    if (m_backend)
        destroy();

    char *error = nullptr;
    QByteArray rootUtf8 = astreaRoot.toUtf8();
    QByteArray localeUtf8 = locale.toUtf8();

    m_backend = astrea_spotlight_backend_create(
        rootUtf8.constData(), localeUtf8.constData(), &error);

    if (!m_backend) {
        if (errorOut && error) {
            *errorOut = QString::fromUtf8(error);
            astrea_spotlight_backend_free_string(error);
        } else if (errorOut) {
            *errorOut = QStringLiteral("Failed to create backend");
        }
        return false;
    }
    m_astreaRoot = astreaRoot;
    m_locale = locale;
    return true;
}

bool RustSpotlightBackend::createWithCatalog(const QString &astreaRoot, const QString &locale,
                                             const QJsonArray &catalog, QString *errorOut) {
    if (m_backend)
        destroy();

    char *error = nullptr;
    QByteArray rootUtf8 = astreaRoot.toUtf8();
    QByteArray localeUtf8 = locale.toUtf8();
    const QByteArray catalogUtf8 = QJsonDocument(catalog).toJson(QJsonDocument::Compact);

    m_backend = astrea_spotlight_backend_create_with_catalog(
        rootUtf8.constData(), localeUtf8.constData(), catalogUtf8.constData(), &error);

    if (!m_backend) {
        if (errorOut && error) {
            *errorOut = QString::fromUtf8(error);
            astrea_spotlight_backend_free_string(error);
        } else if (errorOut) {
            *errorOut = QStringLiteral("Failed to create backend from catalog");
        }
        return false;
    }
    m_astreaRoot = astreaRoot;
    m_locale = locale;
    return true;
}

void RustSpotlightBackend::destroy() {
    if (m_backend) {
        astrea_spotlight_backend_destroy(m_backend);
        m_backend = nullptr;
    }
    m_astreaRoot.clear();
    m_locale.clear();
}

bool RustSpotlightBackend::reload(QString *errorOut) {
    if (!m_backend) {
        if (errorOut) *errorOut = QStringLiteral("Backend not initialized");
        return false;
    }

    char *error = nullptr;
    int ret = astrea_spotlight_backend_reload(m_backend, &error);
    if (ret != 0) {
        if (errorOut && error) {
            *errorOut = QString::fromUtf8(error);
            astrea_spotlight_backend_free_string(error);
        } else if (errorOut) {
            *errorOut = QStringLiteral("Reload failed");
        }
        return false;
    }
    return true;
}

bool RustSpotlightBackend::setCatalog(const QJsonArray &catalog, QString *errorOut) {
    if (!m_backend) {
        if (errorOut) *errorOut = QStringLiteral("Backend not initialized");
        return false;
    }

    char *error = nullptr;
    const QByteArray catalogUtf8 = QJsonDocument(catalog).toJson(QJsonDocument::Compact);
    const int ret = astrea_spotlight_backend_set_catalog_json(
        m_backend, catalogUtf8.constData(), &error);
    if (ret != 0) {
        if (errorOut && error) {
            *errorOut = QString::fromUtf8(error);
            astrea_spotlight_backend_free_string(error);
        } else if (errorOut) {
            *errorOut = QStringLiteral("Set catalog failed");
        }
        return false;
    }
    return true;
}

QJsonArray RustSpotlightBackend::search(const QString &query, int limit, QString *errorOut) {
    if (!m_backend) {
        if (errorOut) *errorOut = QStringLiteral("Backend not initialized");
        return {};
    }

    QByteArray queryUtf8 = query.toUtf8();
    char *error = nullptr;
    char *json = astrea_spotlight_backend_search_json(
        m_backend, queryUtf8.constData(), static_cast<size_t>(limit), &error);

    if (!json) {
        if (errorOut && error) {
            *errorOut = QString::fromUtf8(error);
            astrea_spotlight_backend_free_string(error);
        }
        return {};
    }

    QString jsonStr = QString::fromUtf8(json);
    astrea_spotlight_backend_free_string(json);

    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isArray()) {
        if (errorOut) *errorOut = QStringLiteral("Invalid JSON from backend");
        return {};
    }
    return doc.array();
}

bool RustSpotlightBackend::ensureConfig(QString *errorOut) {
    char *error = nullptr;
    int ret = astrea_spotlight_backend_ensure_config(&error);
    if (ret != 0) {
        if (errorOut && error) {
            *errorOut = QString::fromUtf8(error);
            astrea_spotlight_backend_free_string(error);
        } else if (errorOut) {
            *errorOut = QStringLiteral("Ensure config failed");
        }
        return false;
    }
    return true;
}

QJsonArray RustSpotlightBackend::watchedDirectories() {
    if (!m_backend) return {};
    char *error = nullptr;
    char *json = astrea_spotlight_backend_watched_dirs(m_backend, &error);
    if (!json) {
        if (error) astrea_spotlight_backend_free_string(error);
        return {};
    }
    QString jsonStr = QString::fromUtf8(json);
    astrea_spotlight_backend_free_string(json);
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    return doc.isArray() ? doc.array() : QJsonArray();
}

bool RustSpotlightBackend::recordLaunch(const QString &desktopId, QString *errorOut) {
    if (!m_backend) {
        if (errorOut) *errorOut = QStringLiteral("Backend not initialized");
        return false;
    }

    QByteArray idUtf8 = desktopId.toUtf8();
    char *error = nullptr;
    int ret = astrea_spotlight_backend_record_launch(m_backend, idUtf8.constData(), &error);
    if (ret != 0) {
        if (errorOut && error) {
            *errorOut = QString::fromUtf8(error);
            astrea_spotlight_backend_free_string(error);
        } else if (errorOut) {
            *errorOut = QStringLiteral("Record launch failed");
        }
        return false;
    }
    return true;
}
