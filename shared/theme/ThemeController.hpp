#pragma once

#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>
#include <QStyleHints>
#include <QTimer>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <functional>

class ThemeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int themeMode READ themeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString themePreference READ themePreference NOTIFY themePreferenceChanged)
    Q_PROPERTY(int shellStyle READ shellStyle WRITE setShellStyle NOTIFY shellStyleChanged)
    Q_PROPERTY(int iconStyle READ iconStyle WRITE setIconStyle NOTIFY iconStyleChanged)
    Q_PROPERTY(QString iconTheme READ iconTheme WRITE setIconTheme NOTIFY iconThemeChanged)
    Q_PROPERTY(QString navigationIconTheme READ navigationIconTheme NOTIFY navigationIconThemeChanged)
    Q_PROPERTY(QString navigationIconWeight READ navigationIconWeight NOTIFY navigationIconWeightChanged)
    Q_PROPERTY(int navigationIconOverridesRevision READ navigationIconOverridesRevision NOTIFY navigationIconOverridesChanged)
    Q_PROPERTY(QString visualTheme READ visualTheme NOTIFY visualThemeChanged)
    Q_PROPERTY(QVariantMap styleTokens READ styleTokens NOTIFY visualThemeChanged)
    Q_PROPERTY(QString iconAppearance READ iconAppearance NOTIFY iconAppearanceChanged)
    Q_PROPERTY(QString accentHex READ accentHex NOTIFY accentHexChanged)
    Q_PROPERTY(int audioOsdStyle READ audioOsdStyle WRITE setAudioOsdStyle NOTIFY audioOsdStyleChanged)
    Q_PROPERTY(QString configPath READ configPath CONSTANT)
    Q_PROPERTY(bool loaded READ loaded CONSTANT)

public:
    using ColorSchemeProvider = std::function<Qt::ColorScheme()>;

    explicit ThemeController(const QString &configPath = {}, QObject *parent = nullptr,
                             ColorSchemeProvider colorSchemeProvider = {});

    int themeMode() const;
    void setThemeMode(int value);
    QString themePreference() const;
    void setThemePreference(const QString &value);
    int shellStyle() const;
    void setShellStyle(int value);
    int iconStyle() const;
    void setIconStyle(int value);
    QString iconTheme() const;
    QString navigationIconTheme() const { return m_navigationIconTheme; }
    QString navigationIconWeight() const { return m_navigationIconWeight; }
    int navigationIconOverridesRevision() const { return m_navigationIconOverridesRevision; }
    QString visualTheme() const { return m_visualTheme; }
    QVariantMap styleTokens() const { return m_styleTokens; }
    Q_INVOKABLE QUrl themedNavigationIcon(const QString &key) const;
    void setIconTheme(const QString &value);
    QString iconAppearance() const;
    void setIconAppearance(const QString &value);
    QString accentHex() const;
    void setAccentHex(const QString &value);
    int audioOsdStyle() const;
    void setAudioOsdStyle(int value);
    QString configPath() const;
    bool loaded() const;

    void applyConfig(const QVariantMap &config);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void save();

signals:
    void themeModeChanged();
    void themePreferenceChanged();
    void shellStyleChanged();
    void iconStyleChanged();
    void iconThemeChanged();
    void navigationIconThemeChanged();
    void navigationIconWeightChanged();
    void navigationIconOverridesChanged();
    void visualThemeChanged();
    void iconAppearanceChanged();
    void accentHexChanged();
    void audioOsdStyleChanged();

private slots:
    void handlePlatformColorSchemeChanged();

private:
    static QString normalizedThemePreference(const QString &value);
    static bool isValidThemePreference(const QString &value);
    static QString normalizedIconAppearance(const QString &value);
    static bool isValidIconAppearance(const QString &value);
    void updateEffectiveThemeMode();
    void setEffectiveThemeMode(int value);
    Qt::ColorScheme platformColorScheme() const;
    void scheduleReload();
    void updateWatchPaths();
    void reloadNavigationIcons();

    QString m_configPath;
    ColorSchemeProvider m_colorSchemeProvider;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
    int m_themeMode = 0;
    QString m_themePreference = QStringLiteral("auto");
    int m_shellStyle = 1;
    int m_iconStyle = 0;
    QString m_iconTheme = QStringLiteral("dark");
    QString m_navigationIconTheme = QStringLiteral("lucide");
    QString m_navigationIconWeight = QStringLiteral("regular");
    QJsonObject m_navigationIconOverrides;
    int m_navigationIconOverridesRevision = 0;
    QString m_visualTheme = QStringLiteral("astrea");
    QVariantMap m_styleTokens;
    QString m_iconAppearance = QStringLiteral("default");
    QString m_accentHex = QStringLiteral("#0a84ff");
    int m_audioOsdStyle = 0;
    bool m_loaded = false;
    bool m_reloading = false;
};
