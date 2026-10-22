#pragma once

#include "settings/RegionalPreferences.hpp"
#include <QObject>
#include <QVariantMap>

class SettingsTranslationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap messages READ messages NOTIFY messagesChanged)

public:
    explicit SettingsTranslationController(QObject *parent = nullptr);

    QVariantMap messages() const;
    Q_INVOKABLE QString tr(const QString &key, const QString &fallback = {}) const;

signals:
    void messagesChanged();
private:
    void reload();
    RegionalPreferences m_preferences;
    QVariantMap m_messages;
};
