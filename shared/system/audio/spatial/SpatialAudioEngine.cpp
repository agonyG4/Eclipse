#include "system/audio/spatial/SpatialAudioEngine.hpp"

#include "system/audio/spatial/PipeWireSpatialHost.hpp"

#include <QMetaObject>
#include <QSocketNotifier>

#include <cerrno>
#include <climits>
#include <cstdint>
#include <sys/eventfd.h>
#include <unistd.h>
#include <utility>

namespace Astrea::System {
namespace {

int positiveError(int errorCode) noexcept {
  if (errorCode == INT_MIN)
    return INT_MAX;
  const int normalized = errorCode < 0 ? -errorCode : errorCode;
  return normalized == 0 ? EIO : normalized;
}

QString stateNameFor(SpatialAudioEngine::LifecycleState state) {
  switch (state) {
  case SpatialAudioEngine::LifecycleState::Disabled:
    return QStringLiteral("disabled");
  case SpatialAudioEngine::LifecycleState::Preparing:
    return QStringLiteral("preparing");
  case SpatialAudioEngine::LifecycleState::Running:
    return QStringLiteral("running");
  case SpatialAudioEngine::LifecycleState::Failed:
    return QStringLiteral("failed");
  }
  return QStringLiteral("failed");
}

} // namespace

struct SpatialAudioEngine::HostNotificationChannel {
  int fd = -1;
  std::uint64_t epoch = 0;
  std::unique_ptr<QSocketNotifier> notifier;

  ~HostNotificationChannel() {
    if (notifier)
      notifier->setEnabled(false);
    notifier.reset();
    if (fd >= 0)
      ::close(fd);
  }
};

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
    m_hooks.startHost = [this](SpatialAudioHandle *handle, int notificationFd,
                               int *errorCode) {
      if (!m_host)
        m_host = std::make_unique<PipeWireSpatialHost>();
      return m_host->start(handle, notificationFd, errorCode);
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
  return stateNameFor(lifecycleState());
}

SpatialAudioEngine::LifecycleState
SpatialAudioEngine::lifecycleState() const noexcept {
  if (m_lifecycleState == LifecycleState::Running && hostError() != 0)
    return LifecycleState::Failed;
  return m_lifecycleState;
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
  bool running = isRunning();
  const int currentHostError = hostError();
  if (currentHostError != 0)
    running = false;
  auto state = m_lifecycleState;
  if (state == LifecycleState::Running && currentHostError != 0)
    state = LifecycleState::Failed;

  return {{QStringLiteral("enabled"), m_enabled},
          {QStringLiteral("state"), stateNameFor(state)},
          {QStringLiteral("prepared"), isPrepared()},
          {QStringLiteral("running"), running},
          {QStringLiteral("preparationError"), m_preparationError},
          {QStringLiteral("hostError"), currentHostError},
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
    if (!createHostNotificationChannel(errorCode))
      return false;
  } catch (...) {
    if (errorCode)
      *errorCode = ENOMEM;
    return false;
  }

  const int notificationFd = m_hostNotification->fd;
  m_hostActivationAttempted = true;
  try {
    return m_hooks.startHost(handle, notificationFd, errorCode);
  } catch (...) {
    if (errorCode)
      *errorCode = EIO;
    return false;
  }
}

bool SpatialAudioEngine::createHostNotificationChannel(int *errorCode) {
  auto channel = std::make_unique<HostNotificationChannel>();
  channel->fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (channel->fd < 0) {
    if (errorCode)
      *errorCode = errno != 0 ? errno : EIO;
    return false;
  }

  ++m_activationGeneration;
  if (m_activationGeneration == 0)
    ++m_activationGeneration;
  channel->epoch = m_activationGeneration;
  channel->notifier =
      std::make_unique<QSocketNotifier>(channel->fd, QSocketNotifier::Read);
  const auto epoch = channel->epoch;
  const int notificationFd = channel->fd;
  const auto connection = QObject::connect(
      channel->notifier.get(), &QSocketNotifier::activated, this,
      [this, epoch, notificationFd](QSocketDescriptor, QSocketNotifier::Type) {
        handleHostRuntimeNotification(epoch, notificationFd);
      },
      Qt::DirectConnection);
  if (!connection) {
    if (errorCode)
      *errorCode = EIO;
    return false;
  }

  m_currentHostEpoch = epoch;
  m_hostNotification = std::move(channel);
  if (errorCode)
    *errorCode = 0;
  return true;
}

void SpatialAudioEngine::handleHostRuntimeNotification(std::uint64_t epoch,
                                                       int notificationFd) {
  // An obsolete callback must not drain a later activation's reused FD number.
  if (!m_hostNotification || m_hostNotification->fd != notificationFd ||
      m_hostNotification->epoch != epoch || m_currentHostEpoch != epoch)
    return;

  std::uint64_t notification = 0;
  ssize_t bytesRead = -1;
  do {
    bytesRead = ::read(notificationFd, &notification, sizeof(notification));
  } while (bytesRead < 0 && errno == EINTR);

  if (!m_hostNotification || m_hostNotification->fd != notificationFd ||
      m_hostNotification->epoch != epoch || m_currentHostEpoch != epoch ||
      !m_runtimeStarted || !m_enabled || !m_hostActivationAttempted ||
      m_lifecycleState != LifecycleState::Running)
    return;

  int errorCode = 0;
  if (m_host) {
    const int publishedError = m_host->hostError();
    if (publishedError != 0)
      errorCode = publishedError;
  }
  if (errorCode == 0 &&
      bytesRead == static_cast<ssize_t>(sizeof(notification)) &&
      notification <= static_cast<std::uint64_t>(INT_MAX))
    errorCode = static_cast<int>(notification);
  errorCode = positiveError(errorCode);

  m_hostError = errorCode;
  m_hostRunning = false;
  stopHostAndDestroyHandle();
  m_lifecycleState = LifecycleState::Failed;
}

void SpatialAudioEngine::stopHostAndDestroyHandle() noexcept {
  m_currentHostEpoch = 0;
  if (m_hostNotification && m_hostNotification->notifier)
    m_hostNotification->notifier->setEnabled(false);

  if (m_hostActivationAttempted) {
    m_hostRunning = false;
    try {
      m_hooks.stopHost();
    } catch (...) {
    }
    m_hostActivationAttempted = false;
  }

  m_hostNotification.reset();
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
