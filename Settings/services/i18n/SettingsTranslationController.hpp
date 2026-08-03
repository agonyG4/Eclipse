#pragma once

#include <QObject>
#include <QVariantMap>

class SettingsTranslationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap messages READ messages CONSTANT)

public:
    explicit SettingsTranslationController(QObject *parent = nullptr);

    QVariantMap messages() const;
    Q_INVOKABLE QString tr(const QString &key, const QString &fallback = {}) const;

private:
    QVariantMap m_messages;
};
