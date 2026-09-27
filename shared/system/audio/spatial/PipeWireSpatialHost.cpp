#include "system/audio/spatial/PipeWireSpatialHost.hpp"

#include "system/audio/spatial/PipeWireSpatialHostSupport.hpp"
#include "system/audio/spatial/SpatialAudioCAPI.h"

#include <pipewire/core.h>
#include <pipewire/keys.h>
#include <pipewire/pipewire.h>
#include <pipewire/stream.h>
#include <spa/param/audio/raw-utils.h>
#include <spa/param/latency-utils.h>
#include <spa/pod/builder.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>

namespace Astrea::System {
namespace {

namespace Support = PipeWireSpatialHostSupport;

std::string makeLinkGroup() {
  static std::atomic<std::uint64_t> nextInstance{1};
  const auto instance = nextInstance.fetch_add(1, std::memory_order_relaxed);
  return "astrea-spatial-audio-" + std::to_string(::getpid()) + "-" +
         std::to_string(instance);
}

bool hasCanonicalFormat(const spa_pod *param,
                        const spa_audio_info_raw &expected) noexcept {
  if (!param)
    return false;

  spa_audio_info_raw actual{};
  if (spa_format_audio_raw_parse(param, &actual) < 0 ||
      actual.format != expected.format || actual.rate != expected.rate ||
      actual.channels != expected.channels ||
      SPA_FLAG_IS_SET(actual.flags, SPA_AUDIO_FLAG_UNPOSITIONED))
    return false;

  return std::equal(expected.position, expected.position + expected.channels,
                    actual.position);
}

int positiveError(int errorCode) noexcept {
  if (errorCode == INT_MIN)
    return INT_MAX;
  return errorCode < 0 ? -errorCode : errorCode;
}

} // namespace

PipeWireSpatialHost::CallbackGuard::CallbackGuard(
    PipeWireSpatialHost *host) noexcept
    : m_host(host), m_entered(host && host->enterCallback()) {}

PipeWireSpatialHost::CallbackGuard::~CallbackGuard() {
  if (m_entered)
    m_host->leaveCallback();
}

PipeWireSpatialHost::~PipeWireSpatialHost() { stop(); }

bool PipeWireSpatialHost::start(void *rustEngineHandle, int *errorCode) {
  if (isRunning()) {
    if (errorCode)
      *errorCode = 0;
    return true;
  }
  if (m_loop || m_context || m_core || m_capture || m_playback)
    stop();
  if (!rustEngineHandle) {
    if (errorCode)
      *errorCode = 1;
    return false;
  }

  static std::once_flag pipewireInitialized;
  std::call_once(pipewireInitialized, [] { pw_init(nullptr, nullptr); });

  m_runtimeError.store(0, std::memory_order_release);
  m_captureFormatCompatible.store(false, std::memory_order_relaxed);
  m_playbackFormatCompatible.store(false, std::memory_order_relaxed);
  m_playbackPlanarFloat.store(false, std::memory_order_relaxed);
  m_captureStreaming.store(false, std::memory_order_relaxed);
  m_playbackStreaming.store(false, std::memory_order_relaxed);
  m_pairReady.store(false, std::memory_order_relaxed);
  m_running.store(false, std::memory_order_relaxed);
  m_callbackState.store(0, std::memory_order_release);
  m_rustEngineHandle = rustEngineHandle;
  m_linkGroup = makeLinkGroup();
  m_latencies[SPA_DIRECTION_INPUT] = SPA_LATENCY_INFO(SPA_DIRECTION_INPUT);
  m_latencies[SPA_DIRECTION_OUTPUT] = SPA_LATENCY_INFO(SPA_DIRECTION_OUTPUT);
  m_processLatency = {};

  alignas(8) std::array<std::uint8_t, 1024> captureParamsStorage{};
  alignas(8) std::array<std::uint8_t, 1024> playbackParamsStorage{};
  const spa_pod *captureParams[2]{};
  const spa_pod *playbackParams[2]{};
  const auto mainFormat = Support::canonicalMainFormat();
  const auto playbackFormat = Support::canonicalPlaybackFormat();
  static const pw_stream_events captureEvents = [] {
    pw_stream_events events{};
    events.version = PW_VERSION_STREAM_EVENTS;
    events.state_changed = &PipeWireSpatialHost::captureStateChanged;
    events.param_changed = &PipeWireSpatialHost::captureParamChanged;
    events.process = &PipeWireSpatialHost::captureProcess;
    return events;
  }();
  static const pw_stream_events playbackEvents = [] {
    pw_stream_events events{};
    events.version = PW_VERSION_STREAM_EVENTS;
    events.state_changed = &PipeWireSpatialHost::playbackStateChanged;
    events.param_changed = &PipeWireSpatialHost::playbackParamChanged;
    events.process = &PipeWireSpatialHost::playbackProcess;
    return events;
  }();
  static const pw_core_events coreEvents = [] {
    pw_core_events events{};
    events.version = PW_VERSION_CORE_EVENTS;
    events.error = &PipeWireSpatialHost::coreError;
    return events;
  }();
  bool loopLocked = false;
  Support::StartupStage failedStage = Support::StartupStage::ThreadLoopCreate;

  const auto initialize = [&](Support::StartupStage stage) {
    switch (stage) {
    case Support::StartupStage::ThreadLoopCreate:
      m_loop = pw_thread_loop_new("astrea-spatial-audio", nullptr);
      return m_loop != nullptr;
    case Support::StartupStage::ThreadLoopStart:
      if (pw_thread_loop_start(m_loop) < 0)
        return false;
      m_loopStarted = true;
      return true;
    case Support::StartupStage::ContextCreate:
      pw_thread_loop_lock(m_loop);
      loopLocked = true;
      m_context = pw_context_new(pw_thread_loop_get_loop(m_loop), nullptr, 0);
      return m_context != nullptr;
    case Support::StartupStage::CoreConnect:
      m_core = pw_context_connect(m_context, nullptr, 0);
      if (!m_core)
        return false;
      m_coreListenerAdded =
          pw_core_add_listener(m_core, &m_coreListener, &coreEvents, this) >= 0;
      return m_coreListenerAdded;
    case Support::StartupStage::CaptureStreamCreate: {
      auto *properties = Support::makeMainProperties(m_linkGroup.c_str());
      if (!properties)
        return false;
      m_capture = pw_stream_new(m_core, "Astrea Spatial Audio", properties);
      if (!m_capture)
        return false;
      pw_stream_add_listener(m_capture, &m_captureListener, &captureEvents,
                             this);
      m_captureListenerAdded = true;
      return true;
    }
    case Support::StartupStage::PlaybackStreamCreate: {
      auto *properties = Support::makePlaybackProperties(m_linkGroup.c_str());
      if (!properties)
        return false;
      m_playback = pw_stream_new(m_core, "Astrea playback", properties);
      if (!m_playback)
        return false;
      pw_stream_add_listener(m_playback, &m_playbackListener, &playbackEvents,
                             this);
      m_playbackListenerAdded = true;
      return true;
    }
    case Support::StartupStage::FormatSetup: {
      spa_pod_builder captureBuilder;
      spa_pod_builder_init(&captureBuilder, captureParamsStorage.data(),
                           captureParamsStorage.size());
      captureParams[0] = spa_process_latency_build(
          &captureBuilder, SPA_PARAM_ProcessLatency, &m_processLatency);
      captureParams[1] = spa_format_audio_raw_build(
          &captureBuilder, SPA_PARAM_EnumFormat, &mainFormat);

      spa_pod_builder playbackBuilder;
      spa_pod_builder_init(&playbackBuilder, playbackParamsStorage.data(),
                           playbackParamsStorage.size());
      playbackParams[0] = spa_process_latency_build(
          &playbackBuilder, SPA_PARAM_ProcessLatency, &m_processLatency);
      playbackParams[1] = spa_format_audio_raw_build(
          &playbackBuilder, SPA_PARAM_EnumFormat, &playbackFormat);
      return captureParams[0] && captureParams[1] && playbackParams[0] &&
             playbackParams[1];
    }
    case Support::StartupStage::CaptureConnect: {
      return pw_stream_connect(m_capture, PW_DIRECTION_INPUT, PW_ID_ANY,
                               Support::captureStreamFlags(), captureParams,
                               2) >= 0;
    }
    case Support::StartupStage::PlaybackConnect: {
      return pw_stream_connect(m_playback, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                               Support::playbackStreamFlags(), playbackParams,
                               2) >= 0;
    }
    }
    return false;
  };

  const auto rollback = [&] {
    if (loopLocked) {
      pw_thread_loop_unlock(m_loop);
      loopLocked = false;
    }
    stop();
  };

  if (!Support::runStartupTransaction(initialize, rollback, &failedStage)) {
    const int failureCode = static_cast<int>(failedStage) + 2;
    if (errorCode)
      *errorCode = failureCode;
    return false;
  }

  m_pairReady.store(true, std::memory_order_release);
  m_running.store(true, std::memory_order_release);
  if (loopLocked)
    pw_thread_loop_unlock(m_loop);
  if (errorCode)
    *errorCode = 0;
  return true;
}

void PipeWireSpatialHost::stop() noexcept {
  m_callbackState.fetch_or(CallbackClosingBit, std::memory_order_acq_rel);
  m_pairReady.store(false, std::memory_order_release);
  m_running.store(false, std::memory_order_release);
  m_captureStreaming.store(false, std::memory_order_release);
  m_playbackStreaming.store(false, std::memory_order_release);

  while ((m_callbackState.load(std::memory_order_acquire) &
          CallbackCountMask) != 0)
    std::this_thread::yield();

  const bool lockLoop = m_loop && m_loopStarted;
  if (lockLoop)
    pw_thread_loop_lock(m_loop);

  if (m_capture)
    pw_stream_disconnect(m_capture);
  if (m_playback)
    pw_stream_disconnect(m_playback);

  if (m_captureListenerAdded) {
    spa_hook_remove(&m_captureListener);
    m_captureListenerAdded = false;
  }
  if (m_playbackListenerAdded) {
    spa_hook_remove(&m_playbackListener);
    m_playbackListenerAdded = false;
  }
  if (m_capture) {
    pw_stream_destroy(m_capture);
    m_capture = nullptr;
  }
  if (m_playback) {
    pw_stream_destroy(m_playback);
    m_playback = nullptr;
  }

  m_rustEngineHandle = nullptr;
  m_captureFormatCompatible.store(false, std::memory_order_release);
  m_playbackFormatCompatible.store(false, std::memory_order_release);
  m_playbackPlanarFloat.store(false, std::memory_order_release);

  if (m_coreListenerAdded) {
    spa_hook_remove(&m_coreListener);
    m_coreListenerAdded = false;
  }
  if (m_core) {
    pw_core_disconnect(m_core);
    m_core = nullptr;
  }
  if (m_context) {
    pw_context_destroy(m_context);
    m_context = nullptr;
  }

  if (lockLoop)
    pw_thread_loop_unlock(m_loop);
  if (m_loop) {
    if (m_loopStarted) {
      pw_thread_loop_stop(m_loop);
      m_loopStarted = false;
    }
    pw_thread_loop_destroy(m_loop);
    m_loop = nullptr;
  }
  m_linkGroup.clear();
}

void PipeWireSpatialHost::captureProcess(void *data) noexcept {
  auto *host = static_cast<PipeWireSpatialHost *>(data);
  CallbackGuard callback(host);
  if (!callback || !host->m_pairReady.load(std::memory_order_acquire) ||
      !host->m_playback)
    return;

  const int result = pw_stream_trigger_process(host->m_playback);
  if (result < 0) {
    host->publishRuntimeError(result);
    Support::recyclePendingBuffers({host,
                                    &PipeWireSpatialHost::dequeueCaptureBuffer,
                                    &PipeWireSpatialHost::queueCaptureBuffer});
  }
}

void PipeWireSpatialHost::playbackProcess(void *data) noexcept {
  auto *host = static_cast<PipeWireSpatialHost *>(data);
  CallbackGuard callback(host);
  if (!callback || !host->m_pairReady.load(std::memory_order_acquire))
    return;
  host->processStreams();
}

void PipeWireSpatialHost::captureStateChanged(void *data,
                                              pw_stream_state oldState,
                                              pw_stream_state state,
                                              const char *error) noexcept {
  auto *host = static_cast<PipeWireSpatialHost *>(data);
  if (host)
    host->handleStreamState(true, oldState, state, error);
}

void PipeWireSpatialHost::playbackStateChanged(void *data,
                                               pw_stream_state oldState,
                                               pw_stream_state state,
                                               const char *error) noexcept {
  auto *host = static_cast<PipeWireSpatialHost *>(data);
  if (host)
    host->handleStreamState(false, oldState, state, error);
}

void PipeWireSpatialHost::captureParamChanged(void *data, std::uint32_t id,
                                              const spa_pod *param) noexcept {
  auto *host = static_cast<PipeWireSpatialHost *>(data);
  if (host)
    host->handleStreamParam(true, id, param);
}

void PipeWireSpatialHost::playbackParamChanged(void *data, std::uint32_t id,
                                               const spa_pod *param) noexcept {
  auto *host = static_cast<PipeWireSpatialHost *>(data);
  if (host)
    host->handleStreamParam(false, id, param);
}

void PipeWireSpatialHost::coreError(void *data, std::uint32_t, int, int result,
                                    const char *) noexcept {
  auto *host = static_cast<PipeWireSpatialHost *>(data);
  if (host)
    host->publishRuntimeError(result);
}

void PipeWireSpatialHost::processStreams() noexcept {
  auto *captureBuffer = static_cast<pw_buffer *>(Support::dequeueNewestBuffer(
      {this, &PipeWireSpatialHost::dequeueCaptureBuffer,
       &PipeWireSpatialHost::queueCaptureBuffer}));
  auto *playbackBuffer = pw_stream_dequeue_buffer(m_playback);
  if (!playbackBuffer) {
    if (captureBuffer)
      pw_stream_queue_buffer(m_capture, captureBuffer);
    return;
  }

  auto *capture = captureBuffer ? captureBuffer->buffer : nullptr;
  const bool captureFormatSupported =
      m_captureStreaming.load(std::memory_order_acquire) &&
      m_captureFormatCompatible.load(std::memory_order_acquire);
  const bool playbackFormatSupported =
      m_playbackStreaming.load(std::memory_order_acquire) &&
      m_playbackFormatCompatible.load(std::memory_order_acquire);
  Support::processBufferPair(
      capture, playbackBuffer->buffer, playbackBuffer->requested,
      captureFormatSupported, playbackFormatSupported,
      &PipeWireSpatialHost::processRust, m_rustEngineHandle,
      m_playbackPlanarFloat.load(std::memory_order_acquire));

  if (captureBuffer)
    pw_stream_queue_buffer(m_capture, captureBuffer);
  pw_stream_queue_buffer(m_playback, playbackBuffer);
}

void PipeWireSpatialHost::handleStreamState(bool capture,
                                            pw_stream_state oldState,
                                            pw_stream_state state,
                                            const char *) noexcept {
  auto &streaming = capture ? m_captureStreaming : m_playbackStreaming;
  streaming.store(state == PW_STREAM_STATE_STREAMING,
                  std::memory_order_release);
  if (state == PW_STREAM_STATE_ERROR ||
      (state == PW_STREAM_STATE_UNCONNECTED &&
       oldState != PW_STREAM_STATE_UNCONNECTED))
    publishRuntimeError(EIO);
}

void PipeWireSpatialHost::handleStreamParam(bool capture, std::uint32_t id,
                                            const spa_pod *param) noexcept {
  if (id == SPA_PARAM_Format) {
    handleFormat(capture, param);
    return;
  }
  if (id == SPA_PARAM_Latency || id == SPA_PARAM_ProcessLatency)
    handleLatency(id, param);
}

void PipeWireSpatialHost::handleFormat(bool capture,
                                       const spa_pod *param) noexcept {
  if (capture) {
    m_captureFormatCompatible.store(
        hasCanonicalFormat(param, Support::canonicalMainFormat()),
        std::memory_order_release);
    return;
  }

  spa_audio_info_raw actual{};
  const bool parsed = param && spa_format_audio_raw_parse(param, &actual) >= 0;
  m_playbackPlanarFloat.store(parsed && Support::isPlanarFloatAudio(actual, 2),
                              std::memory_order_release);
  m_playbackFormatCompatible.store(
      parsed && hasCanonicalFormat(param, Support::canonicalPlaybackFormat()),
      std::memory_order_release);
}

void PipeWireSpatialHost::handleLatency(std::uint32_t id,
                                        const spa_pod *param) noexcept {
  if (id == SPA_PARAM_Latency) {
    if (!param)
      return;
    spa_latency_info latency{};
    if (spa_latency_parse(param, &latency) < 0 ||
        (latency.direction != SPA_DIRECTION_INPUT &&
         latency.direction != SPA_DIRECTION_OUTPUT))
      return;
    m_latencies[latency.direction] = latency;
    updateLatency(latency.direction, false);
    return;
  }

  spa_process_latency_info processLatency{};
  if (param && spa_process_latency_parse(param, &processLatency) < 0)
    return;
  if (spa_process_latency_info_compare(&m_processLatency, &processLatency) == 0)
    return;
  m_processLatency = processLatency;
  updateLatency(SPA_DIRECTION_INPUT, true);
  updateLatency(SPA_DIRECTION_OUTPUT, true);
}

void PipeWireSpatialHost::updateLatency(spa_direction direction,
                                        bool includeProcessLatency) noexcept {
  pw_stream *stream = nullptr;
  if (direction == SPA_DIRECTION_INPUT)
    stream = m_capture;
  else if (direction == SPA_DIRECTION_OUTPUT)
    stream = m_playback;
  if (!stream)
    return;

  std::array<std::uint8_t, 1024> storage{};
  spa_pod_builder builder;
  spa_pod_builder_init(&builder, storage.data(), storage.size());
  auto latency = m_latencies[direction];
  spa_process_latency_info_add(&m_processLatency, &latency);
  const spa_pod *params[2] = {
      spa_latency_build(&builder, SPA_PARAM_Latency, &latency), nullptr};
  std::uint32_t count = 1;
  if (includeProcessLatency) {
    params[count++] = spa_process_latency_build(
        &builder, SPA_PARAM_ProcessLatency, &m_processLatency);
  }
  pw_stream_update_params(stream, params, count);
}

void PipeWireSpatialHost::publishRuntimeError(int errorCode) noexcept {
  const int normalized = positiveError(errorCode);
  int expected = 0;
  if (m_runtimeError.compare_exchange_strong(expected, normalized,
                                             std::memory_order_release,
                                             std::memory_order_relaxed))
    m_running.store(false, std::memory_order_release);
}

bool PipeWireSpatialHost::enterCallback() noexcept {
  auto state = m_callbackState.load(std::memory_order_acquire);
  while ((state & CallbackClosingBit) == 0) {
    if ((state & CallbackCountMask) == CallbackCountMask)
      return false;
    if (m_callbackState.compare_exchange_weak(state, state + 1,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire))
      return true;
  }
  return false;
}

void PipeWireSpatialHost::leaveCallback() noexcept {
  m_callbackState.fetch_sub(1, std::memory_order_release);
}

int PipeWireSpatialHost::processRust(void *context, const float *const *inputs,
                                     float *const *outputs,
                                     std::uint32_t frames) noexcept {
  return astrea_spatial_audio_process_planar(
      static_cast<SpatialAudioHandle *>(context), inputs, outputs, frames);
}

void *PipeWireSpatialHost::dequeueCaptureBuffer(void *context) noexcept {
  auto *host = static_cast<PipeWireSpatialHost *>(context);
  return host->m_capture ? pw_stream_dequeue_buffer(host->m_capture) : nullptr;
}

void PipeWireSpatialHost::queueCaptureBuffer(void *context,
                                             void *buffer) noexcept {
  auto *host = static_cast<PipeWireSpatialHost *>(context);
  if (host->m_capture && buffer)
    pw_stream_queue_buffer(host->m_capture, static_cast<pw_buffer *>(buffer));
}

} // namespace Astrea::System
