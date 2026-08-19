#pragma once

#include "system/audio/AudioBackend.hpp"

#include <memory>

namespace Astrea::System {

class PipeWireAudioBackend final : public AudioBackend {
public:
    PipeWireAudioBackend();
    ~PipeWireAudioBackend() override;

    bool start(const Callbacks &callbacks, QString *errorOut) override;
    void stop() override;
    bool setDefaultOutput(quint32 nodeId) override;
    bool setVolume(quint32 nodeId, double linear) override;
    bool setMute(quint32 nodeId, bool muted) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Astrea::System
