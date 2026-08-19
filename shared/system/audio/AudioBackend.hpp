#pragma once

#include "system/audio/AudioOutputModel.hpp"

#include <QString>
#include <QVector>

#include <functional>

namespace Astrea::System {

class AudioBackend {
public:
    struct Callbacks {
        std::function<void(QVector<AudioOutput>, quint32)> outputsChanged;
        std::function<void(bool, bool, QString)> defaultStateChanged;
        std::function<void(double, bool)> volumeChanged;
        std::function<void(QString)> errorChanged;
    };

    virtual ~AudioBackend() = default;
    virtual bool start(const Callbacks &callbacks, QString *errorOut) = 0;
    virtual void stop() = 0;
    virtual bool setDefaultOutput(quint32 nodeId) = 0;
    virtual bool setVolume(quint32 nodeId, double linear) = 0;
    virtual bool setMute(quint32 nodeId, bool muted) = 0;
};

} // namespace Astrea::System
