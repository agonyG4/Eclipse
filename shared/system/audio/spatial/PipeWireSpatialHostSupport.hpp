#pragma once

#include "system/audio/spatial/SpatialAudioBlockBridge.hpp"

#include <pipewire/properties.h>
#include <pipewire/stream.h>
#include <spa/buffer/buffer.h>
#include <spa/param/audio/raw.h>

#include <array>
#include <cstdint>

namespace Astrea::System::PipeWireSpatialHostSupport {

enum class StartupStage : std::uint8_t {
  ThreadLoopCreate,
  ThreadLoopStart,
  ContextCreate,
  CoreConnect,
  CaptureStreamCreate,
  PlaybackStreamCreate,
  FormatSetup,
  CaptureConnect,
  PlaybackConnect,
};

inline constexpr std::array<StartupStage, 9> StartupStages = {
    StartupStage::ThreadLoopCreate,    StartupStage::ThreadLoopStart,
    StartupStage::ContextCreate,       StartupStage::CoreConnect,
    StartupStage::CaptureStreamCreate, StartupStage::PlaybackStreamCreate,
    StartupStage::FormatSetup,         StartupStage::CaptureConnect,
    StartupStage::PlaybackConnect,
};

template <typename Initialize, typename Rollback>
bool runStartupTransaction(Initialize &&initialize, Rollback &&rollback,
                           StartupStage *failedStage) {
  for (const auto stage : StartupStages) {
    if (!initialize(stage)) {
      if (failedStage)
        *failedStage = stage;
      rollback();
      return false;
    }
  }
  return true;
}

struct BufferQueue {
  void *context = nullptr;
  void *(*dequeue)(void *context) noexcept = nullptr;
  void (*queue)(void *context, void *buffer) noexcept = nullptr;
};

void recyclePendingBuffers(BufferQueue queue) noexcept;
void *dequeueNewestBuffer(BufferQueue queue) noexcept;

struct BufferProcessResult {
  std::uint32_t frames = 0;
  bool processed = false;
};

spa_audio_info_raw canonicalMainFormat() noexcept;
spa_audio_info_raw canonicalPlaybackFormat() noexcept;
bool isPlanarFloatAudio(const spa_audio_info_raw &format,
                        std::uint32_t channels) noexcept;
pw_stream_flags captureStreamFlags() noexcept;
pw_stream_flags playbackStreamFlags() noexcept;

pw_properties *makeMainProperties(const char *linkGroup);
pw_properties *makePlaybackProperties(const char *linkGroup);

BufferProcessResult
processBufferPair(const spa_buffer *capture, spa_buffer *playback,
                  std::uint64_t requestedFrames, bool captureFormatSupported,
                  bool playbackFormatSupported,
                  SpatialAudioBlockBridge::ProcessFunction process,
                  void *context, bool playbackPlanarFloat = true) noexcept;

} // namespace Astrea::System::PipeWireSpatialHostSupport
