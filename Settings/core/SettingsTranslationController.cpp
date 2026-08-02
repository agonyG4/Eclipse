#include "core/SettingsTranslationController.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

SettingsTranslationController::SettingsTranslationController(QObject *parent)
    : QObject(parent)
{
    QFile file(QStringLiteral(":/Astrea/Settings/assets/i18n/en_US.json"));
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (document.isObject())
        m_messages = document.object().toVariantMap();
}

QVariantMap SettingsTranslationController::messages() const
{
    return m_messages;
}

QString SettingsTranslationController::tr(const QString &key, const QString &fallback) const
{
    const QVariant value = m_messages.value(key);
    return value.isValid() ? value.toString() : (fallback.isEmpty() ? key : fallback);
}
