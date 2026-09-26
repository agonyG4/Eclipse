#pragma once

#include "system/audio/spatial/SpatialAudioCAPI.h"

#include <cstdint>
#include <memory>

namespace Astrea::System {

class PipeWireSpatialHost;

class SpatialAudioEngine final {
public:
  SpatialAudioEngine();
  ~SpatialAudioEngine();

  SpatialAudioEngine(const SpatialAudioEngine &) = delete;
  SpatialAudioEngine &operator=(const SpatialAudioEngine &) = delete;

  bool isPrepared() const noexcept { return m_handle != nullptr; }
  bool isRunning() const noexcept;
  int preparationError() const noexcept { return m_preparationError; }
  int hostError() const noexcept { return m_hostError; }

  bool start();
  void stop() noexcept;
  int selectHrtf(std::uint8_t presetId,
                 std::uint64_t *generation = nullptr) noexcept;
  bool switchState(SpatialAudioState *state) const noexcept;

private:
  SpatialAudioHandle *m_handle = nullptr;
  std::unique_ptr<PipeWireSpatialHost> m_host;
  int m_preparationError = 0;
  int m_hostError = 0;
};

} // namespace Astrea::System
