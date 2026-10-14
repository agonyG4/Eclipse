#pragma once

#include <QDateTime>
#include <QLocale>
#include <QObject>
#include <QString>
#include <QTimer>

class BarClockService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString timeText READ timeText NOTIFY changed)
    Q_PROPERTY(QString dateText READ dateText NOTIFY changed)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)

public:
    enum class ClockTimeFormat {
        Locale,
        TwelveHour,
        TwentyFourHour,
    };
    Q_ENUM(ClockTimeFormat)

    explicit BarClockService(QObject *parent = nullptr);

    QString timeText() const { return m_timeText; }
    QString dateText() const { return m_dateText; }
    bool isRunning() const { return m_timer.isActive(); }

    static QString formatTime(const QDateTime &dateTime, const QLocale &locale,
                              ClockTimeFormat format = ClockTimeFormat::Locale);
    static QString formatDate(const QDateTime &dateTime, const QLocale &locale);
    static int nextMinuteDelayMs(const QDateTime &dateTime);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    void setLocale(const QLocale &locale);

signals:
    void changed();
    void runningChanged();

private:
    void update();
    void scheduleNextMinute(const QDateTime &now);

    QTimer m_timer;
    QLocale m_locale = QLocale::system();
    ClockTimeFormat m_format = ClockTimeFormat::Locale;
    QString m_timeText;
    QString m_dateText;
};
