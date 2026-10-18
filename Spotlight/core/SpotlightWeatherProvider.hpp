#pragma once

#include <QObject>
#include <QMetaType>
#include <QString>

class SpotlightWeatherProvider : public QObject {
    Q_OBJECT

public:
    enum class Failure {
        NoData,
        InvalidData,
        ConnectionFailed,
    };
    Q_ENUM(Failure)

    explicit SpotlightWeatherProvider(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~SpotlightWeatherProvider() override = default;

    virtual void refresh() = 0;

    // Once cancel() returns, the canceled request cannot emit ready() or failed().
    virtual void cancel() = 0;

signals:
    void ready(int temperature, const QString &city, const QString &condition);
    void failed(SpotlightWeatherProvider::Failure failure);
};

Q_DECLARE_METATYPE(SpotlightWeatherProvider::Failure)
