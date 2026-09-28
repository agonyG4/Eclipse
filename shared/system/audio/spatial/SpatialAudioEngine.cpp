#include "system/audio/spatial/SpatialAudioEngine.hpp"

#include "system/audio/spatial/PipeWireSpatialHost.hpp"

#include <QMetaObject>

#include <cerrno>
#include <utility>

namespace Astrea::System {

struct SpatialAudioEngine::PreparedResult {
  SpatialAudioHandle *handle = nullptr;
  int error = 0;
  std::function<void(SpatialAudioHandle *)> destroy;

  ~PreparedResult() {
    if (handle && destroy)
      destroy(handle);
  }

  SpatialAudioHandle *release() noexcept {
    return std::exchange(handle, nullptr);
  }
};

SpatialAudioEngine::SpatialAudioEngine(LifecycleHooks hooks, QObject *parent)
    : QObject(parent), m_hooks(std::move(hooks)) {
  if (!m_hooks.prepare) {
    m_hooks.prepare = [](int32_t *errorCode) {
      return astrea_spatial_audio_create_default(errorCode);
    };
  }
  if (!m_hooks.destroy)
    m_hooks.destroy = [](SpatialAudioHandle *handle) {
      astrea_spatial_audio_destroy(handle);
    };
  if (!m_hooks.startHost) {
    m_hooks.startHost = [this](SpatialAudioHandle *handle, int *errorCode) {
      if (!m_host)
        m_host = std::make_unique<PipeWireSpatialHost>();
      if (m_host->start(handle, errorCode))
        return true;
      m_host->stop();
      m_host.reset();
      return false;
    };
  }
  if (!m_hooks.stopHost) {
    m_hooks.stopHost = [this] {
      if (!m_host)
        return;
      m_host->stop();
      m_host.reset();
    };
  }
}

SpatialAudioEngine::~SpatialAudioEngine() {
  stop();
  if (m_preparationWorker.joinable())
    m_preparationWorker.join();
}

bool SpatialAudioEngine::isRunning() const noexcept {
  if (!m_hostRunning)
    return false;
  return !m_host || m_host->isRunning();
}

QString SpatialAudioEngine::lifecycleStateName() const {
  switch (m_lifecycleState) {
  case LifecycleState::Disabled:
    return QStringLiteral("disabled");
  case LifecycleState::Preparing:
    return QStringLiteral("preparing");
  case LifecycleState::Running:
    return QStringLiteral("running");
  case LifecycleState::Failed:
    return QStringLiteral("failed");
  }
  return QStringLiteral("failed");
}

int SpatialAudioEngine::hostError() const noexcept {
  if (m_host) {
    const int runtimeError = m_host->hostError();
    if (runtimeError != 0)
      return runtimeError;
  }
  return m_hostError;
}

QJsonObject SpatialAudioEngine::healthJson() const {
  return {{QStringLiteral("enabled"), m_enabled},
          {QStringLiteral("state"), lifecycleStateName()},
          {QStringLiteral("prepared"), isPrepared()},
          {QStringLiteral("running"), isRunning()},
          {QStringLiteral("preparationError"), m_preparationError},
          {QStringLiteral("hostError"), hostError()},
          {QStringLiteral("configurationError"), m_configurationError}};
}

void SpatialAudioEngine::setConfigurationError(QString error) {
  m_configurationError = std::move(error);
}

void SpatialAudioEngine::setEnabled(bool enabled) {
  if (m_enabled == enabled)
    return;

  m_enabled = enabled;
  if (!enabled) {
    stopHostAndDestroyHandle();
    m_preparationError = 0;
    m_hostError = 0;
    m_lifecycleState = LifecycleState::Disabled;
    return;
  }

  m_preparationError = 0;
  m_hostError = 0;
  activate();
}

bool SpatialAudioEngine::start() {
  if (m_runtimeStarted)
    return isRunning();
  m_runtimeStarted = true;
  if (m_enabled)
    activate();
  else
    m_lifecycleState = LifecycleState::Disabled;
  return isRunning();
}

void SpatialAudioEngine::stop() noexcept {
  m_runtimeStarted = false;
  stopHostAndDestroyHandle();
  m_lifecycleState = LifecycleState::Disabled;
}

void SpatialAudioEngine::activate() {
  if (!m_runtimeStarted || !m_enabled) {
    m_lifecycleState = LifecycleState::Disabled;
    return;
  }
  if (m_hostRunning) {
    m_lifecycleState = LifecycleState::Running;
    return;
  }
  if (m_preparationInFlight) {
    m_lifecycleState = LifecycleState::Preparing;
    return;
  }
  if (m_handle) {
    int errorCode = 0;
    if (startHostForHandle(m_handle, &errorCode)) {
      m_hostError = 0;
      m_hostRunning = true;
      m_lifecycleState = LifecycleState::Running;
      return;
    }
    try {
      m_hooks.stopHost();
    } catch (...) {
    }
    m_hostError = errorCode != 0 ? errorCode : EIO;
    stopHostAndDestroyHandle();
    m_lifecycleState = LifecycleState::Failed;
    return;
  }
  beginPreparation();
}

void SpatialAudioEngine::beginPreparation() {
  if (m_preparationInFlight || !m_runtimeStarted || !m_enabled)
    return;

  m_lifecycleState = LifecycleState::Preparing;
  m_preparationError = 0;
  m_hostError = 0;
  m_preparationInFlight = true;

  try {
    const auto prepare = m_hooks.prepare;
    auto result = std::make_shared<PreparedResult>();
    result->destroy = m_hooks.destroy;
    m_preparationWorker = std::thread([this, prepare, result] {
      int errorCode = 0;
      try {
        result->handle = prepare(&errorCode);
      } catch (...) {
        errorCode = EIO;
      }
      result->error = errorCode;
      QMetaObject::invokeMethod(
          this, [this, result] { completePreparation(result); },
          Qt::QueuedConnection);
    });
  } catch (...) {
    m_preparationInFlight = false;
    m_preparationError = EAGAIN;
    m_lifecycleState = LifecycleState::Failed;
  }
}

void SpatialAudioEngine::completePreparation(
    const std::shared_ptr<PreparedResult> &result) {
  if (m_preparationWorker.joinable())
    m_preparationWorker.join();
  m_preparationInFlight = false;

  if (!m_runtimeStarted || !m_enabled) {
    m_lifecycleState = LifecycleState::Disabled;
    return;
  }
  if (!result->handle || result->error != 0) {
    m_preparationError = result->error != 0 ? result->error : EIO;
    m_lifecycleState = LifecycleState::Failed;
    return;
  }

  m_handle = result->release();
  int errorCode = 0;
  if (!startHostForHandle(m_handle, &errorCode)) {
    try {
      m_hooks.stopHost();
    } catch (...) {
    }
    m_hostError = errorCode != 0 ? errorCode : EIO;
    stopHostAndDestroyHandle();
    m_lifecycleState = LifecycleState::Failed;
    return;
  }

  m_hostError = 0;
  m_hostRunning = true;
  m_lifecycleState = LifecycleState::Running;
}

bool SpatialAudioEngine::startHostForHandle(SpatialAudioHandle *handle,
                                            int *errorCode) {
  if (!handle) {
    if (errorCode)
      *errorCode = EINVAL;
    return false;
  }
  try {
    return m_hooks.startHost(handle, errorCode);
  } catch (...) {
    if (errorCode)
      *errorCode = EIO;
    return false;
  }
}

void SpatialAudioEngine::stopHostAndDestroyHandle() noexcept {
  if (m_hostRunning) {
    try {
      m_hooks.stopHost();
    } catch (...) {
    }
    m_hostRunning = false;
  }
  if (m_handle) {
    SpatialAudioHandle *handle = std::exchange(m_handle, nullptr);
    try {
      m_hooks.destroy(handle);
    } catch (...) {
    }
  }
}

int SpatialAudioEngine::selectHrtf(std::uint8_t presetId,
                                   std::uint64_t *generation) noexcept {
  if (!m_handle)
    return 4;
  return astrea_spatial_audio_select(m_handle, presetId, generation);
}

bool SpatialAudioEngine::switchState(SpatialAudioState *state) const noexcept {
  return m_handle && astrea_spatial_audio_read_state(m_handle, state) == 0;
}

} // namespace Astrea::System
