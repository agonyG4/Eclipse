#include "system/audio/spatial/PipeWireSpatialHost.hpp"

#include "system/audio/spatial/SpatialAudioCAPI.h"

#include <pipewire/filter.h>
#include <pipewire/keys.h>
#include <pipewire/pipewire.h>
#include <spa/node/io.h>

#include <algorithm>
#include <array>
#include <limits>
#include <mutex>

namespace Astrea::System {

PipeWireSpatialHost::~PipeWireSpatialHost() { stop(); }

bool PipeWireSpatialHost::start(void *rustEngineHandle, int *errorCode) {
  if (m_filter)
    return true;
  if (!rustEngineHandle) {
    if (errorCode)
      *errorCode = 1;
    return false;
  }

  static std::once_flag pipewireInitialized;
  std::call_once(pipewireInitialized, [] { pw_init(nullptr, nullptr); });

  m_rustEngineHandle = rustEngineHandle;
  m_loop = pw_thread_loop_new("astrea-spatial-audio", nullptr);
  if (!m_loop) {
    m_rustEngineHandle = nullptr;
    if (errorCode)
      *errorCode = 2;
    return false;
  }
  if (pw_thread_loop_start(m_loop) < 0) {
    pw_thread_loop_destroy(m_loop);
    m_loop = nullptr;
    m_rustEngineHandle = nullptr;
    if (errorCode)
      *errorCode = 3;
    return false;
  }

  pw_thread_loop_lock(m_loop);
  pw_filter_events events{};
  events.version = PW_VERSION_FILTER_EVENTS;
  events.process = &PipeWireSpatialHost::process;
  auto *properties = pw_properties_new(
      PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Filter",
      PW_KEY_MEDIA_ROLE, "DSP", PW_KEY_NODE_RATE, "1/48000",
      PW_KEY_NODE_LOCK_RATE, "true", PW_KEY_NODE_DESCRIPTION,
      "Astrea 7.1 to stereo spatial audio", nullptr);
  m_filter =
      pw_filter_new_simple(pw_thread_loop_get_loop(m_loop),
                           "astrea-spatial-audio", properties, &events, this);
  bool portsReady = m_filter != nullptr;
  for (std::uint32_t channel = 0; portsReady && channel < m_inputPorts.size();
       ++channel) {
    const auto name = SpatialAudioBlockBridge::InputChannelNames[channel];
    portsReady = addPort(true, channel, name.data(), name.data());
  }
  for (std::uint32_t channel = 0; portsReady && channel < m_outputPorts.size();
       ++channel) {
    const auto name = SpatialAudioBlockBridge::OutputChannelNames[channel];
    portsReady = addPort(false, channel, name.data(), name.data());
  }
  const int connectResult =
      portsReady
          ? pw_filter_connect(m_filter, PW_FILTER_FLAG_RT_PROCESS, nullptr, 0)
          : -1;
  pw_thread_loop_unlock(m_loop);

  if (connectResult < 0) {
    stop();
    if (errorCode)
      *errorCode = 4;
    return false;
  }
  if (errorCode)
    *errorCode = 0;
  return true;
}

void PipeWireSpatialHost::stop() noexcept {
  if (!m_loop)
    return;

  pw_thread_loop_lock(m_loop);
  if (m_filter) {
    pw_filter_disconnect(m_filter);
    pw_filter_destroy(m_filter);
    m_filter = nullptr;
  }
  m_inputPorts.fill(nullptr);
  m_outputPorts.fill(nullptr);
  m_rustEngineHandle = nullptr;
  pw_thread_loop_unlock(m_loop);
  pw_thread_loop_stop(m_loop);
  pw_thread_loop_destroy(m_loop);
  m_loop = nullptr;
}

void PipeWireSpatialHost::process(void *data,
                                  spa_io_position *position) noexcept {
  auto *host = static_cast<PipeWireSpatialHost *>(data);
  if (!host || !host->m_rustEngineHandle)
    return;

  const auto requestedFrames = position ? position->clock.duration : 0;
  const auto frames = static_cast<std::uint32_t>(std::min<std::uint64_t>(
      requestedFrames, std::numeric_limits<std::uint32_t>::max()));
  const bool supportedRate =
      position && SpatialAudioBlockBridge::supportsClockRate(
                      position->clock.rate.num, position->clock.rate.denom);
  std::array<const float *, 8> inputs{};
  std::array<float *, 2> outputs{};
  for (std::size_t channel = 0; channel < inputs.size(); ++channel) {
    inputs[channel] = static_cast<const float *>(
        pw_filter_get_dsp_buffer(host->m_inputPorts[channel], frames));
  }
  for (std::size_t channel = 0; channel < outputs.size(); ++channel) {
    outputs[channel] = static_cast<float *>(
        pw_filter_get_dsp_buffer(host->m_outputPorts[channel], frames));
  }
  SpatialAudioBlockBridge::process(
      inputs, outputs, frames,
      supportedRate ? &PipeWireSpatialHost::processRust : nullptr,
      host->m_rustEngineHandle);
}

int PipeWireSpatialHost::processRust(void *context, const float *const *inputs,
                                     float *const *outputs,
                                     std::uint32_t frames) noexcept {
  return astrea_spatial_audio_process_planar(
      static_cast<SpatialAudioHandle *>(context), inputs, outputs, frames);
}

bool PipeWireSpatialHost::addPort(bool input, std::uint32_t channel,
                                  const char *name, const char *audioChannel) {
  auto *portData = static_cast<PortData *>(pw_filter_add_port(
      m_filter, input ? PW_DIRECTION_INPUT : PW_DIRECTION_OUTPUT,
      PW_FILTER_PORT_FLAG_MAP_BUFFERS, sizeof(PortData),
      pw_properties_new(PW_KEY_FORMAT_DSP, "32 bit float mono audio",
                        PW_KEY_PORT_NAME, name, PW_KEY_AUDIO_CHANNEL,
                        audioChannel, nullptr),
      nullptr, 0));
  if (!portData)
    return false;
  portData->channel = channel;
  if (input)
    m_inputPorts[channel] = portData;
  else
    m_outputPorts[channel] = portData;
  return true;
}

} // namespace Astrea::System
