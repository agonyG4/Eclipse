#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QFileSystemWatcher>
#include <QTimer>

class AstreaI18n : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)
    Q_PROPERTY(QJsonObject messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

public:
    explicit AstreaI18n(const QString &i18nDir, QObject *parent = nullptr);

    QString language() const { return m_language; }
    QJsonObject messages() const { return m_messages; }
    bool ready() const { return m_ready; }

    Q_INVOKABLE QString tr(const QString &key, const QString &fallback = {},
                           const QVariantMap &params = {}) const;

    Q_INVOKABLE void reload();

signals:
    void languageChanged();
    void messagesChanged();
    void readyChanged();

private:
    void loadCatalogs();
    QString detectLanguage() const;

    QString m_i18nDir;
    QString m_language = QStringLiteral("en_US");
    QJsonObject m_messages;
    QJsonObject m_fallbackMessages;
    QJsonObject m_activeMessages;
    bool m_ready = false;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_reloadDebounce = nullptr;
};
