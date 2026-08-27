#pragma once

#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTimer>

struct DockConfig {
    int iconSize = 48;
    int bottomMargin = 12;
    int panelPadding = 14;
    int itemSpacing = 10;
    QString hoverEffect = QStringLiteral("magnification");
    double magnificationScale = 1.6;
    double magnificationRadius = 2.5;
    QStringList pins;

    static DockConfig defaults();
};

class DockConfigWatcher final : public QObject {
    Q_OBJECT

public:
    explicit DockConfigWatcher(const QString &configPath, const QString &componentsPath,
                               QObject *parent = nullptr);

    DockConfig config() const { return m_config; }
    bool componentEnabled() const;
    QString configPath() const { return m_configPath; }
    QString componentsPath() const { return m_componentsPath; }
    quint64 revision() const { return m_revision; }
    QString lastError() const { return m_lastError; }

signals:
    void configChanged();
    void componentToggled(bool enabled);

public slots:
    void refresh();

private:
    struct JsonResult {
        QJsonObject object;
        QString error;
    };

    JsonResult loadJsonFile(const QString &path) const;
    DockConfig parseConfig(const QJsonObject &object, QStringList *errors) const;
    void addPathWithParents(const QString &path);
    static int integerField(const QJsonObject &object, const QString &key, int fallback,
                            int minimum, int maximum, QStringList *errors);
    static bool booleanField(const QJsonObject &object, const QString &key, bool fallback,
                             QStringList *errors);
    static double doubleField(const QJsonObject &object, const QString &key, double fallback,
                              double minimum, double maximum, QStringList *errors);

    QString m_configPath;
    QString m_componentsPath;
    DockConfig m_config;
    QJsonObject m_componentsConfig;
    QString m_lastError;
    quint64 m_revision = 0;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
