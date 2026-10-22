#include "services/i18n/SettingsTranslationController.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

SettingsTranslationController::SettingsTranslationController(QObject *parent)
    : QObject(parent)
{
    connect(&m_preferences, &RegionalPreferences::changed, this, &SettingsTranslationController::reload);
    reload();
}

void SettingsTranslationController::reload()
{
    QLocale::setDefault(QLocale(m_preferences.language()));
    QVariantMap messages;
    const auto load = [&messages](const QString &language) {
        QFile file(QStringLiteral(":/Astrea/Settings/assets/i18n/%1.json").arg(language));
        if (!file.open(QIODevice::ReadOnly)) return;
        const auto document=QJsonDocument::fromJson(file.readAll());
        if (!document.isObject()) return;
        const auto localized=document.object().toVariantMap();
        for(auto it=localized.cbegin();it!=localized.cend();++it) messages[it.key()]=it.value();
    };
    load("en_US");
    if(m_preferences.language()!="en_US") load(m_preferences.language());
    if(m_messages==messages) return;
    m_messages=messages;
    emit messagesChanged();
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
