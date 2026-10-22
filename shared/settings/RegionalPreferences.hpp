#pragma once
#include <QFileSystemWatcher>
#include <QLocale>
#include <QObject>
#include <QTimer>
class RegionalPreferences final : public QObject
{
    Q_OBJECT
  public:
    explicit RegionalPreferences(QObject *parent = nullptr);
    explicit RegionalPreferences(QString path, QObject *parent = nullptr);
    QString language() const { return m_language; }
    QLocale locale() const;
    QString timeFormat() const { return m_timeFormat; }
    QString path() const { return m_path; }
    void reload();
  signals:
    void changed();

  private:
    QString m_path;
    QString m_language = "en_US";
    QString m_country = "BR";
    QString m_timeFormat = "24h";
    QFileSystemWatcher m_watcher;
    QTimer m_fallback;
};
