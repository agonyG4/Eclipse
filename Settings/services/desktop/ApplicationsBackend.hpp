#pragma once
#include "NativeSettingsSupport.hpp"
#include "apps/DesktopEntryParser.hpp"
#include <QVariantList>
class ApplicationsBackend
{
  public:
    ApplicationsBackend(QString dataHome, QStringList dataDirs, QString desktop,
                        NativeSettings::Runner runner = NativeSettings::run);
    static ApplicationsBackend system();
    QVariantList inventory() const;
    QVariantMap details(const QString &id) const;
    QString location(const QString &id) const;
    void shortcut(const QString &id) const;
    void uninstall(const QString &id, const QString &expectedTarget) const;

  private:
    QList<DesktopEntryRecord> records() const;
    DesktopEntryRecord resolve(const QString &id) const;
    QString m_dataHome;
    QStringList m_dataDirs;
    QString m_desktop;
    NativeSettings::Runner m_runner;
};
