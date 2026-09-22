#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QStyleHints>
#include <QTimer>
#include <QString>
#include <QVariantMap>

#include <functional>

class ThemeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int themeMode READ themeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString themePreference READ themePreference NOTIFY themePreferenceChanged)
    Q_PROPERTY(int shellStyle READ shellStyle WRITE setShellStyle NOTIFY shellStyleChanged)
    Q_PROPERTY(int iconStyle READ iconStyle WRITE setIconStyle NOTIFY iconStyleChanged)
    Q_PROPERTY(QString iconTheme READ iconTheme WRITE setIconTheme NOTIFY iconThemeChanged)
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
    void setIconTheme(const QString &value);
    QString iconAppearance() const;
    void setIconAppearance(const QString &value);
    QString accentHex() const;
    void setAccentHex(const QString &value);
    int audioOsdStyle() const;
    void setAudioOsdStyle(int value);
    QString configPath() const;
    bool loaded() const;

    Q_INVOKABLE void applyConfig(const QVariantMap &config);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void save();

signals:
    void themeModeChanged();
    void themePreferenceChanged();
    void shellStyleChanged();
    void iconStyleChanged();
    void iconThemeChanged();
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

    QString m_configPath;
    ColorSchemeProvider m_colorSchemeProvider;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
    int m_themeMode = 0;
    QString m_themePreference = QStringLiteral("auto");
    int m_shellStyle = 1;
    int m_iconStyle = 0;
    QString m_iconTheme = QStringLiteral("dark");
    QString m_iconAppearance = QStringLiteral("default");
    QString m_accentHex = QStringLiteral("#0a84ff");
    int m_audioOsdStyle = 0;
    bool m_loaded = false;
    bool m_reloading = false;
};
