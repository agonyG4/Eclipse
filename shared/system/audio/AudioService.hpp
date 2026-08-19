#pragma once

#include "system/SystemServiceState.hpp"
#include "system/audio/AudioBackend.hpp"

#include <QJsonObject>
#include <QObject>
#include <QTimer>

#include <memory>

namespace Astrea::System {

class AudioOutputModel;

class AudioService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(SystemServiceState state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool available READ available NOTIFY healthChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY healthChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(double volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged)
    Q_PROPERTY(AudioOutputModel *outputsModel READ outputsModel CONSTANT)

public:
    explicit AudioService(std::unique_ptr<AudioBackend> backend = {}, QObject *parent = nullptr);
    ~AudioService() override;

    SystemServiceState state() const { return m_state; }
    bool available() const { return m_available; }
    bool ready() const { return m_ready; }
    QString errorString() const { return m_errorString; }
    double volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    AudioOutputModel *outputsModel() const { return m_outputsModel; }

    bool start();
    void stop();
    Q_INVOKABLE void adjustVolume(double deltaPercent);
    Q_INVOKABLE void setMuted(bool muted);
    Q_INVOKABLE bool setDefaultOutput(quint32 nodeId);
    QJsonObject healthJson() const;

    static double uiPercentToLinear(double percent);
    static double linearToUiPercent(double linear);

signals:
    void stateChanged();
    void healthChanged();
    void errorStringChanged();
    void volumeChanged();
    void mutedChanged();

private:
    void setState(SystemServiceState state);
    void setErrorString(const QString &errorString);
    void applyOutputs(QVector<AudioOutput> outputs, quint32 defaultNodeId);
    void applyDefaultState(bool available, bool ready, const QString &errorString);
    AudioBackend::Callbacks callbacksForGeneration(quint64 generation);
    void startBackend();
    void scheduleReconnect();

    std::unique_ptr<AudioBackend> m_backend;
    AudioOutputModel *m_outputsModel = nullptr;
    SystemServiceState m_state = SystemServiceState::Stopped;
    bool m_available = false;
    bool m_ready = false;
    QString m_errorString;
    double m_volume = 100.0;
    bool m_muted = false;
    quint64 m_generation = 0;
    bool m_wantsRunning = false;
    int m_reconnectAttempt = 0;
    QTimer m_reconnectTimer;
};

} // namespace Astrea::System
