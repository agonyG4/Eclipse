#include "system/audio/spatial/SpatialAudioEngine.hpp"

#include "system/audio/spatial/PipeWireSpatialHost.hpp"

namespace Astrea::System {

SpatialAudioEngine::SpatialAudioEngine()
    : m_handle(astrea_spatial_audio_create_default(&m_preparationError)) {}

SpatialAudioEngine::~SpatialAudioEngine() {
  stop();
  astrea_spatial_audio_destroy(m_handle);
  m_handle = nullptr;
}

bool SpatialAudioEngine::isRunning() const noexcept {
  return m_host && m_host->isRunning();
}

bool SpatialAudioEngine::start() {
  if (!m_handle)
    return false;
  if (!m_host)
    m_host = std::make_unique<PipeWireSpatialHost>();
  return m_host->start(m_handle, &m_hostError);
}

void SpatialAudioEngine::stop() noexcept {
  if (m_host)
    m_host->stop();
}

int SpatialAudioEngine::selectHrtf(std::uint8_t presetId,
                                   std::uint64_t *generation) noexcept {
  if (!m_handle)
    return 4;
  return astrea_spatial_audio_select(m_handle, presetId, generation);
}

bool SpatialAudioEngine::switchState(SpatialAudioState *state) const noexcept {
  return m_handle && astrea_spatial_audio_read_state(m_handle, state) == 0;
}

} // namespace Astrea::System
