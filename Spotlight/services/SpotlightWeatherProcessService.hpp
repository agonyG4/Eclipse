#pragma once

#include "core/SpotlightWeatherProvider.hpp"

#include <QPointer>
#include <QString>

class QProcess;

class SpotlightWeatherProcessService final : public SpotlightWeatherProvider {
    Q_OBJECT

public:
    explicit SpotlightWeatherProcessService(QString executable,
                                            QObject *parent = nullptr);
    ~SpotlightWeatherProcessService() override;

    void refresh() override;
    void cancel() override;

private:
    QString m_executable;
    QPointer<QProcess> m_process;
    quint64 m_requestGeneration = 0;
};
