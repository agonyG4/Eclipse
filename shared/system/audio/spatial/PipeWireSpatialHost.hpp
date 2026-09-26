#pragma once

#include "system/audio/spatial/SpatialAudioBlockBridge.hpp"

#include <array>
#include <cstdint>

struct pw_filter;
struct pw_thread_loop;
struct spa_io_position;

namespace Astrea::System {

class PipeWireSpatialHost final {
public:
  PipeWireSpatialHost() = default;
  ~PipeWireSpatialHost();

  PipeWireSpatialHost(const PipeWireSpatialHost &) = delete;
  PipeWireSpatialHost &operator=(const PipeWireSpatialHost &) = delete;

  bool start(void *rustEngineHandle, int *errorCode = nullptr);
  void stop() noexcept;
  bool isRunning() const noexcept { return m_filter != nullptr; }

private:
  struct PortData {
    std::uint32_t channel = 0;
  };

  static void process(void *data, spa_io_position *position) noexcept;
  static int processRust(void *context, const float *const *inputs,
                         float *const *outputs, std::uint32_t frames) noexcept;
  bool addPort(bool input, std::uint32_t channel, const char *name,
               const char *audioChannel);

  void *m_rustEngineHandle = nullptr;
  pw_thread_loop *m_loop = nullptr;
  pw_filter *m_filter = nullptr;
  std::array<void *, 8> m_inputPorts{};
  std::array<void *, 2> m_outputPorts{};
};

} // namespace Astrea::System
