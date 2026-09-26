#include "system/audio/spatial/SpatialAudioBlockBridge.hpp"

#include <algorithm>

namespace Astrea::System {

bool SpatialAudioBlockBridge::process(
    const std::array<const float *, 8> &inputs,
    const std::array<float *, 2> &outputs, std::uint32_t frames,
    ProcessFunction function, void *context) noexcept {
  if (!outputs[0] || !outputs[1]) {
    if (outputs[0])
      std::fill_n(outputs[0], frames, 0.0f);
    if (outputs[1])
      std::fill_n(outputs[1], frames, 0.0f);
    return false;
  }

  if (!function ||
      !std::all_of(inputs.begin(), inputs.end(),
                   [](const float *input) { return input != nullptr; })) {
    std::fill_n(outputs[0], frames, 0.0f);
    std::fill_n(outputs[1], frames, 0.0f);
    return false;
  }

  if (function(context, inputs.data(), outputs.data(), frames) == 0)
    return true;

  std::fill_n(outputs[0], frames, 0.0f);
  std::fill_n(outputs[1], frames, 0.0f);
  return false;
}

} // namespace Astrea::System
