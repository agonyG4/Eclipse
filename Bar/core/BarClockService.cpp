#include "core/BarClockService.hpp"

#include <QTime>

BarClockService::BarClockService(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &BarClockService::update);
}

QString BarClockService::formatTime(const QDateTime &dateTime, const QLocale &locale,
                                    ClockTimeFormat format)
{
    switch (format) {
    case ClockTimeFormat::TwelveHour:
        return locale.toString(dateTime.time(), QStringLiteral("hh:mm AP"));
    case ClockTimeFormat::TwentyFourHour:
        return locale.toString(dateTime.time(), QStringLiteral("HH:mm"));
    case ClockTimeFormat::Locale:
        return locale.toString(dateTime.time(), QLocale::ShortFormat);
    }
    return locale.toString(dateTime.time(), QLocale::ShortFormat);
}

QString BarClockService::formatDate(const QDateTime &dateTime, const QLocale &locale)
{
    return locale.toString(dateTime.date(), QStringLiteral("ddd MMM d"));
}

int BarClockService::nextMinuteDelayMs(const QDateTime &dateTime)
{
    if (!dateTime.isValid())
        return 60 * 1000;

    QDateTime next = dateTime;
    next.setTime(QTime(dateTime.time().hour(), dateTime.time().minute(), 0, 0));
    next = next.addSecs(60);
    return qMax(250, dateTime.msecsTo(next) + 20);
}

void BarClockService::start()
{
    const bool wasRunning = m_timer.isActive();
    update();
    if (!wasRunning)
        emit runningChanged();
}

void BarClockService::stop()
{
    if (!m_timer.isActive())
        return;
    m_timer.stop();
    emit runningChanged();
}

void BarClockService::setLocale(const QLocale &locale)
{
    if (m_locale == locale)
        return;
    m_locale = locale;
    if (!m_timeText.isEmpty())
        update();
}

void BarClockService::update()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QString nextTime = formatTime(now, m_locale, m_format);
    const QString nextDate = formatDate(now, m_locale);
    const bool changed = nextTime != m_timeText || nextDate != m_dateText;
    m_timeText = nextTime;
    m_dateText = nextDate;
    if (changed)
        emit this->changed();
    scheduleNextMinute(now);
}

void BarClockService::scheduleNextMinute(const QDateTime &now)
{
    m_timer.start(nextMinuteDelayMs(now));
}
