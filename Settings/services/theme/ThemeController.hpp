#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class ThemeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(int shellStyle READ shellStyle WRITE setShellStyle NOTIFY shellStyleChanged)
    Q_PROPERTY(int iconStyle READ iconStyle WRITE setIconStyle NOTIFY iconStyleChanged)
    Q_PROPERTY(QString iconTheme READ iconTheme WRITE setIconTheme NOTIFY iconThemeChanged)
    Q_PROPERTY(QString accentHex READ accentHex WRITE setAccentHex NOTIFY accentHexChanged)
    Q_PROPERTY(int audioOsdStyle READ audioOsdStyle WRITE setAudioOsdStyle NOTIFY audioOsdStyleChanged)
    Q_PROPERTY(QString configPath READ configPath CONSTANT)
    Q_PROPERTY(bool loaded READ loaded CONSTANT)

public:
    explicit ThemeController(const QString &configPath = {}, QObject *parent = nullptr);

    int themeMode() const;
    void setThemeMode(int value);
    int shellStyle() const;
    void setShellStyle(int value);
    int iconStyle() const;
    void setIconStyle(int value);
    QString iconTheme() const;
    void setIconTheme(const QString &value);
    QString accentHex() const;
    void setAccentHex(const QString &value);
    int audioOsdStyle() const;
    void setAudioOsdStyle(int value);
    QString configPath() const;
    bool loaded() const;

    Q_INVOKABLE void applyConfig(const QVariantMap &config);
    Q_INVOKABLE void save();

signals:
    void themeModeChanged();
    void shellStyleChanged();
    void iconStyleChanged();
    void iconThemeChanged();
    void accentHexChanged();
    void audioOsdStyleChanged();

private:
    void load();

    QString m_configPath;
    int m_themeMode = 0;
    int m_shellStyle = 0;
    int m_iconStyle = 0;
    QString m_iconTheme = QStringLiteral("dark");
    QString m_accentHex = QStringLiteral("#0a84ff");
    int m_audioOsdStyle = 0;
    bool m_loaded = false;
};
