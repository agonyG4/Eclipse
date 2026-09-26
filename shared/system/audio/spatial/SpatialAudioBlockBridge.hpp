#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace Astrea::System {

class SpatialAudioBlockBridge final {
public:
  static constexpr std::array<std::string_view, 8> InputChannelNames = {
      "FL", "FR", "FC", "LFE", "RL", "RR", "SL", "SR"};
  static constexpr std::array<std::string_view, 2> OutputChannelNames = {"FL",
                                                                         "FR"};

  using ProcessFunction = int (*)(void *context, const float *const *inputs,
                                  float *const *outputs, std::uint32_t frames);

  static constexpr bool supportsClockRate(std::uint32_t numerator,
                                          std::uint32_t denominator) noexcept {
    return numerator != 0 && static_cast<std::uint64_t>(denominator) ==
                                 static_cast<std::uint64_t>(numerator) * 48'000;
  }

  static bool process(const std::array<const float *, 8> &inputs,
                      const std::array<float *, 2> &outputs,
                      std::uint32_t frames, ProcessFunction function,
                      void *context) noexcept;
};

} // namespace Astrea::System
