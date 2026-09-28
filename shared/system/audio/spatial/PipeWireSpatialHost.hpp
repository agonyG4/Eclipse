#pragma once

#include "system/audio/spatial/SpatialAudioBlockBridge.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

#include <pipewire/stream.h>
#include <spa/param/latency.h>
#include <spa/utils/hook.h>

struct pw_context;
struct pw_core;
struct pw_stream;
struct pw_thread_loop;
struct spa_pod;
class PipeWireSpatialHostTest;

namespace Astrea::System {

class PipeWireSpatialHost final {
public:
  PipeWireSpatialHost() = default;
  ~PipeWireSpatialHost();

  PipeWireSpatialHost(const PipeWireSpatialHost &) = delete;
  PipeWireSpatialHost &operator=(const PipeWireSpatialHost &) = delete;

  bool start(void *rustEngineHandle, int runtimeErrorNotifyFd,
             int *errorCode = nullptr);
  void stop() noexcept;
  bool isRunning() const noexcept {
    return m_running.load(std::memory_order_acquire);
  }
  int hostError() const noexcept {
    return m_runtimeError.load(std::memory_order_acquire);
  }

private:
  friend class ::PipeWireSpatialHostTest;

  static constexpr std::uint32_t CallbackClosingBit = 1U << 31U;
  static constexpr std::uint32_t CallbackCountMask = ~CallbackClosingBit;

  class CallbackGuard final {
  public:
    explicit CallbackGuard(PipeWireSpatialHost *host) noexcept;
    ~CallbackGuard();
    CallbackGuard(const CallbackGuard &) = delete;
    CallbackGuard &operator=(const CallbackGuard &) = delete;
    explicit operator bool() const noexcept { return m_entered; }

  private:
    PipeWireSpatialHost *m_host = nullptr;
    bool m_entered = false;
  };

  static void captureProcess(void *data) noexcept;
  static void playbackProcess(void *data) noexcept;
  static void captureStateChanged(void *data, pw_stream_state oldState,
                                  pw_stream_state state,
                                  const char *error) noexcept;
  static void playbackStateChanged(void *data, pw_stream_state oldState,
                                   pw_stream_state state,
                                   const char *error) noexcept;
  static void captureParamChanged(void *data, std::uint32_t id,
                                  const spa_pod *param) noexcept;
  static void playbackParamChanged(void *data, std::uint32_t id,
                                   const spa_pod *param) noexcept;
  static void coreError(void *data, std::uint32_t id, int sequence, int result,
                        const char *message) noexcept;

  void processStreams() noexcept;
  void handleStreamState(bool capture, pw_stream_state oldState,
                         pw_stream_state state, const char *error) noexcept;
  void handleStreamParam(bool capture, std::uint32_t id,
                         const spa_pod *param) noexcept;
  void handleFormat(bool capture, const spa_pod *param) noexcept;
  void handleLatency(std::uint32_t id, const spa_pod *param) noexcept;
  void updateLatency(spa_direction direction,
                     bool includeProcessLatency) noexcept;
  void publishRuntimeError(int errorCode) noexcept;
  bool enterCallback() noexcept;
  void leaveCallback() noexcept;

  static int processRust(void *context, const float *const *inputs,
                         float *const *outputs, std::uint32_t frames) noexcept;
  static void *dequeueCaptureBuffer(void *context) noexcept;
  static void queueCaptureBuffer(void *context, void *buffer) noexcept;

  void *m_rustEngineHandle = nullptr;
  pw_thread_loop *m_loop = nullptr;
  pw_context *m_context = nullptr;
  pw_core *m_core = nullptr;
  pw_stream *m_capture = nullptr;
  pw_stream *m_playback = nullptr;
  spa_hook m_coreListener{};
  spa_hook m_captureListener{};
  spa_hook m_playbackListener{};
  bool m_loopStarted = false;
  bool m_coreListenerAdded = false;
  bool m_captureListenerAdded = false;
  bool m_playbackListenerAdded = false;
  std::string m_linkGroup;
  std::array<spa_latency_info, 2> m_latencies{};
  spa_process_latency_info m_processLatency{};
  std::atomic<std::uint32_t> m_callbackState{CallbackClosingBit};
  std::atomic<bool> m_captureFormatCompatible{false};
  std::atomic<bool> m_playbackFormatCompatible{false};
  std::atomic<bool> m_playbackPlanarFloat{false};
  std::atomic<bool> m_captureStreaming{false};
  std::atomic<bool> m_playbackStreaming{false};
  std::atomic<bool> m_pairReady{false};
  std::atomic<bool> m_running{false};
  std::atomic<int> m_runtimeError{0};
  // Borrowed from SpatialAudioEngine; the owner keeps it open through stop().
  int m_runtimeErrorNotifyFd = -1;
};

} // namespace Astrea::System
