#include "system/audio/spatial/PipeWireSpatialHostSupport.hpp"

#include <pipewire/keys.h>
#include <spa/param/audio/raw.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace Astrea::System::PipeWireSpatialHostSupport {
namespace {

constexpr std::uint32_t kSampleRate = 48'000;
constexpr std::uint32_t kInputChannels = 8;
constexpr std::uint32_t kOutputChannels = 2;
constexpr std::uint32_t kSampleSize = sizeof(float);

struct ReadPlane {
  const float *samples = nullptr;
  std::uint32_t frames = 0;
};

struct WritePlane {
  float *samples = nullptr;
  std::uint32_t capacity = 0;
};

bool readPlane(const spa_data &data, ReadPlane &plane) noexcept {
  plane = {};
  if (!data.data || !data.chunk || (data.flags & SPA_DATA_FLAG_READABLE) == 0 ||
      (data.chunk->flags & SPA_CHUNK_FLAG_CORRUPTED) != 0 ||
      data.chunk->stride != static_cast<std::int32_t>(kSampleSize) ||
      data.chunk->offset > data.maxsize ||
      data.chunk->size > data.maxsize - data.chunk->offset ||
      data.chunk->offset % alignof(float) != 0 ||
      reinterpret_cast<std::uintptr_t>(data.data) % alignof(float) != 0)
    return false;

  const auto *bytes = static_cast<const std::byte *>(data.data);
  plane.samples = reinterpret_cast<const float *>(bytes + data.chunk->offset);
  plane.frames = data.chunk->size / kSampleSize;
  return true;
}

bool writePlane(spa_data &data, WritePlane &plane) noexcept {
  plane = {};
  if (!data.data || !data.chunk || (data.flags & SPA_DATA_FLAG_WRITABLE) == 0 ||
      reinterpret_cast<std::uintptr_t>(data.data) % alignof(float) != 0)
    return false;

  plane.samples = static_cast<float *>(data.data);
  plane.capacity = data.maxsize / kSampleSize;
  return true;
}

pw_properties *newAudioProperties(const char *linkGroup, const char *nodeName,
                                  const char *description,
                                  const char *mediaClass,
                                  const char *channelCount,
                                  const char *position) {
  if (!linkGroup || linkGroup[0] == '\0')
    return nullptr;

  return pw_properties_new(
      PW_KEY_NODE_NAME, nodeName, PW_KEY_NODE_DESCRIPTION, description,
      PW_KEY_MEDIA_CLASS, mediaClass, PW_KEY_NODE_GROUP, linkGroup,
      PW_KEY_NODE_LINK_GROUP, linkGroup, PW_KEY_AUDIO_FORMAT, "F32P",
      PW_KEY_AUDIO_RATE, "48000", PW_KEY_AUDIO_CHANNELS, channelCount,
      SPA_KEY_AUDIO_POSITION, position, nullptr);
}

} // namespace

void recyclePendingBuffers(BufferQueue queue) noexcept {
  if (!queue.dequeue || !queue.queue)
    return;
  while (void *buffer = queue.dequeue(queue.context))
    queue.queue(queue.context, buffer);
}

void *dequeueNewestBuffer(BufferQueue queue) noexcept {
  if (!queue.dequeue || !queue.queue)
    return nullptr;

  void *newest = nullptr;
  while (void *buffer = queue.dequeue(queue.context)) {
    if (newest)
      queue.queue(queue.context, newest);
    newest = buffer;
  }
  return newest;
}

void queueBufferPair(BufferQueue captureQueue, void *captureBuffer,
                     BufferQueue playbackQueue, void *playbackBuffer) noexcept {
  if (captureBuffer && captureQueue.queue)
    captureQueue.queue(captureQueue.context, captureBuffer);
  if (playbackBuffer && playbackQueue.queue)
    playbackQueue.queue(playbackQueue.context, playbackBuffer);
}

spa_audio_info_raw canonicalMainFormat() noexcept {
  spa_audio_info_raw info{};
  info.format = SPA_AUDIO_FORMAT_F32P;
  info.rate = kSampleRate;
  info.channels = kInputChannels;
  const std::array<std::uint32_t, kInputChannels> positions = {
      SPA_AUDIO_CHANNEL_FL,  SPA_AUDIO_CHANNEL_FR, SPA_AUDIO_CHANNEL_FC,
      SPA_AUDIO_CHANNEL_LFE, SPA_AUDIO_CHANNEL_RL, SPA_AUDIO_CHANNEL_RR,
      SPA_AUDIO_CHANNEL_SL,  SPA_AUDIO_CHANNEL_SR};
  std::copy(positions.begin(), positions.end(), info.position);
  return info;
}

spa_audio_info_raw canonicalPlaybackFormat() noexcept {
  spa_audio_info_raw info{};
  info.format = SPA_AUDIO_FORMAT_F32P;
  info.rate = kSampleRate;
  info.channels = kOutputChannels;
  info.position[0] = SPA_AUDIO_CHANNEL_FL;
  info.position[1] = SPA_AUDIO_CHANNEL_FR;
  return info;
}

bool isPlanarFloatAudio(const spa_audio_info_raw &format,
                        std::uint32_t channels) noexcept {
  return format.format == SPA_AUDIO_FORMAT_F32P && format.channels == channels;
}

pw_stream_flags captureStreamFlags() noexcept {
  return static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                      PW_STREAM_FLAG_MAP_BUFFERS |
                                      PW_STREAM_FLAG_RT_PROCESS |
                                      PW_STREAM_FLAG_ASYNC);
}

pw_stream_flags playbackStreamFlags() noexcept {
  return static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                      PW_STREAM_FLAG_MAP_BUFFERS |
                                      PW_STREAM_FLAG_RT_PROCESS |
                                      PW_STREAM_FLAG_TRIGGER);
}

pw_properties *makeMainProperties(const char *linkGroup) {
  auto *properties = newAudioProperties(linkGroup, "astrea_spatial_audio",
                                        "Astrea Spatial Audio", "Audio/Sink",
                                        "8", "[ FL FR FC LFE RL RR SL SR ]");
  if (!properties)
    return nullptr;

  if (pw_properties_set(properties, PW_KEY_NODE_VIRTUAL, "true") < 0 ||
      pw_properties_set(properties, "filter.smart", "true") < 0 ||
      pw_properties_set(properties, "filter.smart.name",
                        "astrea-spatial-audio") < 0 ||
      pw_properties_set(properties, "filter.smart.targetable", "true") < 0) {
    pw_properties_free(properties);
    return nullptr;
  }
  return properties;
}

pw_properties *makePlaybackProperties(const char *linkGroup) {
  auto *properties = newAudioProperties(
      linkGroup, "astrea_spatial_audio.playback", "Astrea playback",
      "Stream/Output/Audio", "2", "[ FL FR ]");
  if (!properties)
    return nullptr;

  if (pw_properties_set(properties, PW_KEY_NODE_PASSIVE, "true") < 0 ||
      pw_properties_set(properties, PW_KEY_STREAM_DONT_REMIX, "true") < 0) {
    pw_properties_free(properties);
    return nullptr;
  }
  return properties;
}

BufferProcessResult
processBufferPair(const spa_buffer *capture, spa_buffer *playback,
                  std::uint64_t requestedFrames, bool captureFormatSupported,
                  bool playbackFormatSupported,
                  SpatialAudioBlockBridge::ProcessFunction process,
                  void *context, bool playbackPlanarFloat) noexcept {
  BufferProcessResult result;
  if (!playbackPlanarFloat || !playback || !playback->datas ||
      playback->n_datas != kOutputChannels)
    return result;

  std::array<WritePlane, kOutputChannels> outputPlanes{};
  std::uint32_t outputCapacity = std::numeric_limits<std::uint32_t>::max();
  for (std::size_t channel = 0; channel < outputPlanes.size(); ++channel) {
    if (!writePlane(playback->datas[channel], outputPlanes[channel]))
      return result;
    outputCapacity = std::min(outputCapacity, outputPlanes[channel].capacity);
  }

  const auto boundedRequest = static_cast<std::uint32_t>(
      std::min<std::uint64_t>(requestedFrames, outputCapacity));

  std::array<ReadPlane, kInputChannels> inputPlanes{};
  std::array<const float *, kInputChannels> inputs{};
  bool captureValid =
      capture && capture->datas && capture->n_datas == kInputChannels;
  std::uint32_t captureCapacity = std::numeric_limits<std::uint32_t>::max();
  if (captureValid) {
    for (std::size_t channel = 0; channel < inputPlanes.size(); ++channel) {
      if (!readPlane(capture->datas[channel], inputPlanes[channel])) {
        captureValid = false;
        break;
      }
      captureCapacity = std::min(captureCapacity, inputPlanes[channel].frames);
    }
  }

  const bool hasCaptureFrames = captureValid && captureCapacity != 0;
  const std::uint32_t frames = hasCaptureFrames
                                   ? std::min(outputCapacity, captureCapacity)
                                   : boundedRequest;
  if (hasCaptureFrames) {
    for (std::size_t channel = 0; channel < inputPlanes.size(); ++channel)
      inputs[channel] = inputPlanes[channel].samples;
  }

  result.frames = frames;
  for (std::size_t channel = 0; channel < outputPlanes.size(); ++channel) {
    auto &chunk = *playback->datas[channel].chunk;
    chunk.offset = 0;
    chunk.size = frames * kSampleSize;
    chunk.stride = static_cast<std::int32_t>(kSampleSize);
  }

  std::array<float *, kOutputChannels> outputs = {outputPlanes[0].samples,
                                                  outputPlanes[1].samples};
  const bool shouldProcess = captureFormatSupported &&
                             playbackFormatSupported && hasCaptureFrames &&
                             process && frames != 0;
  result.processed = SpatialAudioBlockBridge::process(
      inputs, outputs, frames, shouldProcess ? process : nullptr, context);
  return result;
}

} // namespace Astrea::System::PipeWireSpatialHostSupport
