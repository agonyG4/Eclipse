#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// Topbar look-and-feel preferences shared by the Bar (reader) and Settings
// (writer). Stored in ~/.config/AstreaOS/topbar.json and watched, so a change
// made in Settings reaches the running Bar without a restart.
class TopbarPreferences : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString configPath READ configPath WRITE setConfigPath NOTIFY configPathChanged)
    Q_PROPERTY(QString popupStyle READ popupStyle WRITE setPopupStyle NOTIFY popupStyleChanged)
    Q_PROPERTY(bool attached READ attached NOTIFY popupStyleChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit TopbarPreferences(QObject *parent = nullptr);

    static QString defaultConfigPath();
    static QString normalizedPopupStyle(const QString &style);

    QString configPath() const { return m_configPath; }
    void setConfigPath(const QString &path);
    QString popupStyle() const { return m_popupStyle; }
    void setPopupStyle(const QString &style);
    bool attached() const { return m_popupStyle == QStringLiteral("attached"); }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE void reload();

signals:
    void configPathChanged();
    void popupStyleChanged();
    void lastErrorChanged();

private:
    void watch();
    bool persist();
    void setLastError(const QString &error);
    void applyPopupStyle(const QString &style);

    QString m_configPath;
    QString m_popupStyle = QStringLiteral("floating");
    QString m_lastError;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadDebounce;
};
