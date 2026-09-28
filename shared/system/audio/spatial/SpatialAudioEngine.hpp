#pragma once

#include "system/audio/spatial/SpatialAudioCAPI.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

namespace Astrea::System {

class PipeWireSpatialHost;

class SpatialAudioEngine final : public QObject {
public:
  enum class LifecycleState { Disabled, Preparing, Running, Failed };

  // These four functions are the narrow lifecycle seam used by deterministic
  // tests.
  struct LifecycleHooks {
    std::function<SpatialAudioHandle *(int32_t *)> prepare;
    std::function<void(SpatialAudioHandle *)> destroy;
    std::function<bool(SpatialAudioHandle *, int *)> startHost;
    std::function<void()> stopHost;
  };

  explicit SpatialAudioEngine(LifecycleHooks hooks = {},
                              QObject *parent = nullptr);
  ~SpatialAudioEngine();

  SpatialAudioEngine(const SpatialAudioEngine &) = delete;
  SpatialAudioEngine &operator=(const SpatialAudioEngine &) = delete;

  bool isPrepared() const noexcept { return m_handle != nullptr; }
  bool isRunning() const noexcept;
  bool enabled() const noexcept { return m_enabled; }
  LifecycleState lifecycleState() const noexcept { return m_lifecycleState; }
  QString lifecycleStateName() const;
  int preparationError() const noexcept { return m_preparationError; }
  int hostError() const noexcept;
  QJsonObject healthJson() const;
  void setConfigurationError(QString error);
  QString configurationError() const { return m_configurationError; }

  void setEnabled(bool enabled);
  bool start();
  void stop() noexcept;
  int selectHrtf(std::uint8_t presetId,
                 std::uint64_t *generation = nullptr) noexcept;
  bool switchState(SpatialAudioState *state) const noexcept;

private:
  struct PreparedResult;

  void activate();
  void beginPreparation();
  void completePreparation(const std::shared_ptr<PreparedResult> &result);
  bool startHostForHandle(SpatialAudioHandle *handle, int *errorCode);
  void stopHostAndDestroyHandle() noexcept;

  LifecycleHooks m_hooks;
  SpatialAudioHandle *m_handle = nullptr;
  std::unique_ptr<PipeWireSpatialHost> m_host;
  std::thread m_preparationWorker;
  LifecycleState m_lifecycleState = LifecycleState::Disabled;
  bool m_enabled = false;
  bool m_runtimeStarted = false;
  bool m_preparationInFlight = false;
  bool m_hostRunning = false;
  int m_preparationError = 0;
  int m_hostError = 0;
  QString m_configurationError;
};

} // namespace Astrea::System
