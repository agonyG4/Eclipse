#include "system/audio/AudioService.hpp"

#include "system/audio/AudioOutputModel.hpp"
#include "system/audio/PipeWireAudioBackend.hpp"

#include <QMetaObject>
#include <QPointer>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace Astrea::System {

AudioService::AudioService(std::unique_ptr<AudioBackend> backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend ? std::move(backend)
                        : std::make_unique<PipeWireAudioBackend>())
    , m_outputsModel(new AudioOutputModel(this))
{
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this] {
        if (!m_wantsRunning || m_state == SystemServiceState::Stopped)
            return;
        ++m_attemptGeneration;
        m_backend->stop();
        startBackend();
    });
}

AudioService::~AudioService()
{
    stop();
}

bool AudioService::start()
{
    if (m_state != SystemServiceState::Stopped)
        return true;
    m_wantsRunning = true;
    ++m_generation;
    setState(SystemServiceState::Starting);
    setErrorString({});
    startBackend();
    return true;
}

AudioBackend::Callbacks AudioService::callbacksForGeneration(quint64 generation,
                                                              quint64 attemptGeneration)
{
    const QPointer<AudioService> self(this);
    AudioBackend::Callbacks callbacks;
    callbacks.outputsChanged = [self, generation, attemptGeneration](QVector<AudioOutput> outputs,
                                                   quint32 defaultNodeId) {
        if (!self)
            return;
        QMetaObject::invokeMethod(self, [self, generation, attemptGeneration,
                                          outputs = std::move(outputs),
                                          defaultNodeId]() mutable {
            if (self && self->m_generation == generation
                && self->m_attemptGeneration == attemptGeneration
                && self->m_state != SystemServiceState::Stopped)
                self->applyOutputs(std::move(outputs), defaultNodeId);
        }, Qt::QueuedConnection);
    };
    callbacks.defaultStateChanged = [self, generation, attemptGeneration](bool available, bool ready,
                                                        QString errorString) {
        if (!self)
            return;
        QMetaObject::invokeMethod(self, [self, generation, attemptGeneration, available, ready,
                                          errorString = std::move(errorString)] {
            if (self && self->m_generation == generation
                && self->m_attemptGeneration == attemptGeneration
                && self->m_state != SystemServiceState::Stopped)
                self->applyDefaultState(available, ready, errorString);
        }, Qt::QueuedConnection);
    };
    callbacks.volumeChanged = [self, generation, attemptGeneration](double linear, bool muted) {
        if (!self)
            return;
        QMetaObject::invokeMethod(self, [self, generation, attemptGeneration, linear, muted] {
            if (!self || self->m_generation != generation
                || self->m_attemptGeneration != attemptGeneration
                || self->m_state == SystemServiceState::Stopped)
                return;
            const double volume = AudioService::linearToUiPercent(linear);
            if (!qFuzzyCompare(self->m_volume, volume)) {
                self->m_volume = volume;
                emit self->volumeChanged();
            }
            if (self->m_muted != muted) {
                self->m_muted = muted;
                emit self->mutedChanged();
            }
            if (!self->m_defaultStateAvailable) {
                self->m_defaultStateAvailable = true;
                emit self->defaultStateAvailableChanged();
            }
        }, Qt::QueuedConnection);
    };
    callbacks.errorChanged = [self, generation, attemptGeneration](QString errorString) {
        if (!self)
            return;
        QMetaObject::invokeMethod(self, [self, generation, attemptGeneration,
                                          errorString = std::move(errorString)] {
            if (!self || self->m_generation != generation
                || self->m_attemptGeneration != attemptGeneration
                || self->m_state == SystemServiceState::Stopped)
                return;
            self->setErrorString(errorString);
            self->setState(SystemServiceState::Degraded);
            self->scheduleReconnect();
        }, Qt::QueuedConnection);
    };
    return callbacks;
}

void AudioService::startBackend()
{
    const quint64 generation = m_generation;
    const quint64 attemptGeneration = ++m_attemptGeneration;
    const AudioBackend::Callbacks callbacks = callbacksForGeneration(generation, attemptGeneration);
    QString error;
    if (!m_backend->start(callbacks, &error)) {
        setErrorString(error.isEmpty() ? QStringLiteral("Audio backend unavailable") : error);
        m_available = false;
        m_ready = false;
        emit healthChanged();
        setState(SystemServiceState::Unavailable);
        scheduleReconnect();
        return;
    }
    m_reconnectTimer.stop();
    m_reconnectAttempt = 0;
    m_available = true;
    m_ready = true;
    emit healthChanged();
    setState(SystemServiceState::Ready);
}

void AudioService::scheduleReconnect()
{
    if (!m_wantsRunning || m_reconnectTimer.isActive())
        return;
    static constexpr int delays[] = {250, 500, 1000, 2000, 5000};
    const int index = std::min(m_reconnectAttempt,
                               static_cast<int>(std::size(delays) - 1));
    m_reconnectTimer.start(delays[index]);
    ++m_reconnectAttempt;
}

void AudioService::stop()
{
    if (m_state == SystemServiceState::Stopped)
        return;
    ++m_generation;
    ++m_attemptGeneration;
    m_wantsRunning = false;
    m_reconnectTimer.stop();
    m_reconnectAttempt = 0;
    m_backend->stop();
    m_outputsModel->replace({}, 0);
    m_defaultNodeId = 0;
    if (m_defaultStateAvailable) {
        m_defaultStateAvailable = false;
        emit defaultStateAvailableChanged();
    }
    m_available = false;
    m_ready = false;
    emit healthChanged();
    setState(SystemServiceState::Stopped);
}

void AudioService::adjustVolume(double deltaPercent)
{
    const double next = std::clamp(m_volume + deltaPercent, 0.0, 150.0);
    if (qFuzzyCompare(next, m_volume))
        return;
    if (!m_defaultNodeId || !m_defaultStateAvailable
        || !m_backend->setVolume(m_defaultNodeId, uiPercentToLinear(next)))
        return;
    m_volume = next;
    emit volumeChanged();
}

void AudioService::setMuted(bool muted)
{
    if (m_muted == muted)
        return;
    if (!m_defaultNodeId || !m_defaultStateAvailable
        || !m_backend->setMute(m_defaultNodeId, muted))
        return;
    m_muted = muted;
    emit mutedChanged();
}

bool AudioService::setDefaultOutput(quint32 nodeId)
{
    return m_backend->setDefaultOutput(nodeId);
}

QJsonObject AudioService::healthJson() const
{
    QJsonObject result = serviceHealthJson(m_state, m_available, m_ready, m_errorString);
    result.insert(QStringLiteral("muted"), m_muted);
    result.insert(QStringLiteral("volume"), m_volume);
    result.insert(QStringLiteral("defaultStateAvailable"), m_defaultStateAvailable);
    return result;
}

double AudioService::uiPercentToLinear(double percent)
{
    const double clamped = std::clamp(percent, 0.0, 150.0) / 100.0;
    return clamped * clamped * clamped;
}

double AudioService::linearToUiPercent(double linear)
{
    return std::clamp(std::cbrt(std::max(0.0, linear)) * 100.0, 0.0, 150.0);
}

void AudioService::setState(SystemServiceState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
    emit healthChanged();
}

void AudioService::setErrorString(const QString &errorString)
{
    if (m_errorString == errorString)
        return;
    m_errorString = errorString;
    emit errorStringChanged();
    emit healthChanged();
}

void AudioService::applyOutputs(QVector<AudioOutput> outputs, quint32 defaultNodeId)
{
    if (m_defaultNodeId != defaultNodeId) {
        m_defaultNodeId = defaultNodeId;
        if (m_defaultStateAvailable) {
            m_defaultStateAvailable = false;
            emit defaultStateAvailableChanged();
        }
    }
    m_outputsModel->replace(std::move(outputs), defaultNodeId);
}

void AudioService::applyDefaultState(bool available, bool ready,
                                     const QString &errorString)
{
    m_available = available;
    m_ready = ready;
    if (!ready && m_defaultStateAvailable) {
        m_defaultStateAvailable = false;
        emit defaultStateAvailableChanged();
    }
    setErrorString(errorString);
    setState(ready ? SystemServiceState::Ready
                   : available ? SystemServiceState::Degraded
                               : SystemServiceState::Unavailable);
    if (ready) {
        m_reconnectTimer.stop();
        m_reconnectAttempt = 0;
    } else {
        scheduleReconnect();
    }
    emit healthChanged();
}

} // namespace Astrea::System
