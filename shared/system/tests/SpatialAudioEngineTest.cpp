#include "system/audio/spatial/SpatialAudioEngine.hpp"

#include <QTest>

#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <cstdint>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <sys/eventfd.h>
#include <unistd.h>
#include <vector>

using Astrea::System::SpatialAudioEngine;

namespace {

struct LifecycleProbe {
  std::atomic<int> prepareCalls{0};
  std::atomic<int> destroyCalls{0};
  std::atomic<int> hostStartCalls{0};
  std::atomic<int> hostStopCalls{0};
  std::atomic<bool> preparationStarted{false};
  bool blockPreparation = false;
  bool failPreparation = false;
  bool failHostStart = false;
  bool writeDuringHostStop = false;
  bool notificationFdOpenDuringHostStop = false;
  ssize_t stopWriteResult = -1;
  std::vector<int> notificationFds;
  std::mutex mutex;
  std::condition_variable condition;
  bool releasePreparation = false;
  std::vector<std::string> events;

  void record(const char *event) {
    std::lock_guard lock(mutex);
    events.emplace_back(event);
  }

  void release() {
    {
      std::lock_guard lock(mutex);
      releasePreparation = true;
    }
    condition.notify_all();
  }

  std::vector<std::string> recordedEvents() {
    std::lock_guard lock(mutex);
    return events;
  }
};

SpatialAudioEngine::LifecycleHooks hooksFor(LifecycleProbe &probe) {
  SpatialAudioEngine::LifecycleHooks hooks;
  hooks.prepare = [&probe](int32_t *errorCode) {
    ++probe.prepareCalls;
    probe.preparationStarted.store(true, std::memory_order_release);
    probe.condition.notify_all();
    if (probe.blockPreparation) {
      std::unique_lock lock(probe.mutex);
      probe.condition.wait(lock, [&probe] { return probe.releasePreparation; });
    }
    if (probe.failPreparation) {
      if (errorCode)
        *errorCode = 42;
      return static_cast<SpatialAudioHandle *>(nullptr);
    }
    if (errorCode)
      *errorCode = 0;
    return reinterpret_cast<SpatialAudioHandle *>(new int(1));
  };
  hooks.destroy = [&probe](SpatialAudioHandle *handle) {
    probe.record("destroy");
    ++probe.destroyCalls;
    delete reinterpret_cast<int *>(handle);
  };
  hooks.startHost = [&probe](SpatialAudioHandle *handle, int notificationFd,
                             int *errorCode) {
    probe.record("start");
    ++probe.hostStartCalls;
    probe.notificationFds.push_back(notificationFd);
    if (!handle)
      return false;
    if (probe.failHostStart) {
      if (errorCode)
        *errorCode = 73;
      return false;
    }
    if (errorCode)
      *errorCode = 0;
    return true;
  };
  hooks.stopHost = [&probe] {
    probe.record("stop");
    ++probe.hostStopCalls;
    if (!probe.notificationFds.empty()) {
      const int notificationFd = probe.notificationFds.back();
      probe.notificationFdOpenDuringHostStop =
          ::fcntl(notificationFd, F_GETFD) != -1;
      if (probe.writeDuringHostStop) {
        const std::uint64_t lateError = EIO;
        probe.stopWriteResult =
            ::write(notificationFd, &lateError, sizeof(lateError));
      }
    }
  };
  return hooks;
}

bool writeRuntimeError(int notificationFd, int errorCode) {
  const auto error = static_cast<std::uint64_t>(errorCode);
  return ::write(notificationFd, &error, sizeof(error)) ==
         static_cast<ssize_t>(sizeof(error));
}

} // namespace

class SpatialAudioEngineTest final : public QObject {
  Q_OBJECT

private slots:
  void constructionAndDisabledStartRemainLightweight();
  void enablePreparesOnceAndStartsOneHost();
  void disableStopsHostBeforeDestroyAndIsIdempotent();
  void stopReleasesResourcesAndRestartPreparesAgain();
  void disableDuringPreparationDiscardsTheResult();
  void rapidTogglesReuseTheInFlightPreparation();
  void preparationFailureDoesNotStartHost();
  void hostFailureDestroysPreparedHandle();
  void runtimeHostFailureTearsDownAndPreservesHealth();
  void queuedFailureIsIgnoredAfterDisable();
  void queuedFailureIsIgnoredAfterRuntimeStop();
  void staleFailureDoesNotStopTheNewActivation();
  void destructionJoinsPreparationAndDestroysItsResult();
};

void SpatialAudioEngineTest::constructionAndDisabledStartRemainLightweight() {
  LifecycleProbe probe;
  SpatialAudioEngine engine(hooksFor(probe));

  QVERIFY(!engine.enabled());
  QVERIFY(!engine.isPrepared());
  QVERIFY(!engine.start());
  QCOMPARE(engine.lifecycleStateName(), QStringLiteral("disabled"));
  QCOMPARE(probe.prepareCalls.load(), 0);
  QCOMPARE(probe.hostStartCalls.load(), 0);
  const QJsonObject health = engine.healthJson();
  QVERIFY(!health.value(QStringLiteral("enabled")).toBool());
  QCOMPARE(health.value(QStringLiteral("state")).toString(),
           QStringLiteral("disabled"));
  QVERIFY(!health.value(QStringLiteral("prepared")).toBool());
  QVERIFY(!health.value(QStringLiteral("running")).toBool());
  QCOMPARE(health.value(QStringLiteral("preparationError")).toInt(), 0);
  QCOMPARE(health.value(QStringLiteral("hostError")).toInt(), 0);

  engine.setConfigurationError(QStringLiteral("invalid JSON"));
  QCOMPARE(engine.healthJson()
               .value(QStringLiteral("configurationError"))
               .toString(),
           QStringLiteral("invalid JSON"));
}

void SpatialAudioEngineTest::enablePreparesOnceAndStartsOneHost() {
  LifecycleProbe probe;
  probe.blockPreparation = true;
  SpatialAudioEngine engine(hooksFor(probe));
  engine.start();
  engine.setEnabled(true);

  QTRY_VERIFY_WITH_TIMEOUT(
      probe.preparationStarted.load(std::memory_order_acquire), 3000);
  QCOMPARE(engine.lifecycleStateName(), QStringLiteral("preparing"));
  probe.release();
  QTRY_COMPARE_WITH_TIMEOUT(engine.lifecycleStateName(),
                            QStringLiteral("running"), 3000);
  QVERIFY(engine.enabled());
  QVERIFY(engine.isPrepared());
  QVERIFY(engine.isRunning());

  engine.setEnabled(true);
  engine.start();
  QCOMPARE(probe.prepareCalls.load(), 1);
  QCOMPARE(probe.hostStartCalls.load(), 1);
  QCOMPARE(probe.notificationFds.size(), std::size_t(1));
  const int notificationFd = probe.notificationFds.back();
  QVERIFY(notificationFd >= 0);
  QVERIFY((::fcntl(notificationFd, F_GETFL) & O_NONBLOCK) != 0);
  QVERIFY((::fcntl(notificationFd, F_GETFD) & FD_CLOEXEC) != 0);
  const QJsonObject health = engine.healthJson();
  QCOMPARE(health.value(QStringLiteral("state")).toString(),
           QStringLiteral("running"));
  QVERIFY(health.value(QStringLiteral("prepared")).toBool());
  QVERIFY(health.value(QStringLiteral("running")).toBool());
}

void SpatialAudioEngineTest::disableStopsHostBeforeDestroyAndIsIdempotent() {
  LifecycleProbe probe;
  SpatialAudioEngine engine(hooksFor(probe));
  engine.start();
  engine.setEnabled(true);
  QTRY_COMPARE_WITH_TIMEOUT(engine.lifecycleStateName(),
                            QStringLiteral("running"), 3000);

  engine.setEnabled(false);
  QCOMPARE(engine.lifecycleStateName(), QStringLiteral("disabled"));
  QVERIFY(!engine.isPrepared());
  QVERIFY(!engine.isRunning());
  QCOMPARE(engine.selectHrtf(0), 4);
  SpatialAudioState state{};
  QVERIFY(!engine.switchState(&state));
  const auto events = probe.recordedEvents();
  QCOMPARE(events.size(), std::size_t(3));
  QCOMPARE(QString::fromStdString(events.at(0)), QStringLiteral("start"));
  QCOMPARE(QString::fromStdString(events.at(1)), QStringLiteral("stop"));
  QCOMPARE(QString::fromStdString(events.at(2)), QStringLiteral("destroy"));
  QVERIFY(probe.notificationFdOpenDuringHostStop);
  errno = 0;
  QCOMPARE(::fcntl(probe.notificationFds.front(), F_GETFD), -1);
  QCOMPARE(errno, EBADF);

  engine.setEnabled(false);
  QCOMPARE(probe.hostStopCalls.load(), 1);
  QCOMPARE(probe.destroyCalls.load(), 1);
  QCOMPARE(engine.hostError(), 0);
}

void SpatialAudioEngineTest::stopReleasesResourcesAndRestartPreparesAgain() {
  LifecycleProbe probe;
  SpatialAudioEngine engine(hooksFor(probe));
  engine.setEnabled(true);
  QVERIFY(!engine.isPrepared());

  engine.start();
  QTRY_COMPARE_WITH_TIMEOUT(engine.lifecycleStateName(),
                            QStringLiteral("running"), 3000);
  engine.stop();
  QVERIFY(engine.enabled());
  QVERIFY(!engine.isPrepared());
  QCOMPARE(engine.lifecycleStateName(), QStringLiteral("disabled"));
  QCOMPARE(engine.hostError(), 0);
  QCOMPARE(probe.hostStopCalls.load(), 1);
  QCOMPARE(probe.destroyCalls.load(), 1);
  QVERIFY(probe.notificationFdOpenDuringHostStop);
  const int stoppedNotificationFd = probe.notificationFds.front();
  errno = 0;
  QCOMPARE(::fcntl(stoppedNotificationFd, F_GETFD), -1);
  QCOMPARE(errno, EBADF);

  engine.start();
  QTRY_COMPARE_WITH_TIMEOUT(engine.lifecycleStateName(),
                            QStringLiteral("running"), 3000);
  QCOMPARE(probe.prepareCalls.load(), 2);
  QCOMPARE(probe.hostStartCalls.load(), 2);
}

void SpatialAudioEngineTest::disableDuringPreparationDiscardsTheResult() {
  LifecycleProbe probe;
  probe.blockPreparation = true;
  SpatialAudioEngine engine(hooksFor(probe));
  engine.start();
  engine.setEnabled(true);
  QTRY_VERIFY_WITH_TIMEOUT(
      probe.preparationStarted.load(std::memory_order_acquire), 3000);

  engine.setEnabled(false);
  probe.release();
  QTRY_COMPARE_WITH_TIMEOUT(probe.destroyCalls.load(), 1, 3000);
  QCOMPARE(probe.hostStartCalls.load(), 0);
  QCOMPARE(engine.lifecycleStateName(), QStringLiteral("disabled"));
  QVERIFY(!engine.isPrepared());
}

void SpatialAudioEngineTest::rapidTogglesReuseTheInFlightPreparation() {
  LifecycleProbe probe;
  probe.blockPreparation = true;
  SpatialAudioEngine engine(hooksFor(probe));
  engine.start();
  engine.setEnabled(true);
  QTRY_VERIFY_WITH_TIMEOUT(
      probe.preparationStarted.load(std::memory_order_acquire), 3000);

  engine.setEnabled(false);
  engine.setEnabled(true);
  QCOMPARE(probe.prepareCalls.load(), 1);
  probe.release();
  QTRY_COMPARE_WITH_TIMEOUT(engine.lifecycleStateName(),
                            QStringLiteral("running"), 3000);
  QCOMPARE(probe.prepareCalls.load(), 1);
  QCOMPARE(probe.hostStartCalls.load(), 1);
  QCOMPARE(probe.destroyCalls.load(), 0);
}

void SpatialAudioEngineTest::preparationFailureDoesNotStartHost() {
  LifecycleProbe probe;
  probe.failPreparation = true;
  SpatialAudioEngine engine(hooksFor(probe));
  engine.start();
  engine.setEnabled(true);

  QTRY_COMPARE_WITH_TIMEOUT(engine.lifecycleStateName(),
                            QStringLiteral("failed"), 3000);
  QVERIFY(engine.enabled());
  QVERIFY(!engine.isPrepared());
  QVERIFY(!engine.isRunning());
  QCOMPARE(engine.preparationError(), 42);
  QCOMPARE(probe.hostStartCalls.load(), 0);
}

void SpatialAudioEngineTest::hostFailureDestroysPreparedHandle() {
  LifecycleProbe probe;
  probe.failHostStart = true;
  SpatialAudioEngine engine(hooksFor(probe));
  engine.start();
  engine.setEnabled(true);

  QTRY_COMPARE_WITH_TIMEOUT(engine.lifecycleStateName(),
                            QStringLiteral("failed"), 3000);
  QVERIFY(!engine.isPrepared());
  QVERIFY(!engine.isRunning());
  QCOMPARE(engine.hostError(), 73);
  QCOMPARE(probe.hostStartCalls.load(), 1);
  QCOMPARE(probe.hostStopCalls.load(), 1);
  QCOMPARE(probe.destroyCalls.load(), 1);
}

void SpatialAudioEngineTest::runtimeHostFailureTearsDownAndPreservesHealth() {
  LifecycleProbe probe;
  SpatialAudioEngine engine(hooksFor(probe));
  engine.start();
  engine.setEnabled(true);
  QTRY_COMPARE_WITH_TIMEOUT(engine.lifecycleStateName(),
                            QStringLiteral("running"), 3000);
  QCOMPARE(probe.notificationFds.size(), std::size_t(1));

  QVERIFY(writeRuntimeError(probe.notificationFds.front(), ENETDOWN));
  QTRY_COMPARE_WITH_TIMEOUT(engine.lifecycleStateName(),
                            QStringLiteral("failed"), 3000);

  QVERIFY(engine.enabled());
  QVERIFY(!engine.isPrepared());
  QVERIFY(!engine.isRunning());
  QCOMPARE(engine.hostError(), ENETDOWN);
  QCOMPARE(probe.hostStopCalls.load(), 1);
  QCOMPARE(probe.destroyCalls.load(), 1);
  QVERIFY(probe.notificationFdOpenDuringHostStop);
  const auto events = probe.recordedEvents();
  QCOMPARE(events.size(), std::size_t(3));
  QCOMPARE(QString::fromStdString(events.at(0)), QStringLiteral("start"));
  QCOMPARE(QString::fromStdString(events.at(1)), QStringLiteral("stop"));
  QCOMPARE(QString::fromStdString(events.at(2)), QStringLiteral("destroy"));

  const QJsonObject health = engine.healthJson();
  QCOMPARE(health.value(QStringLiteral("state")).toString(),
           QStringLiteral("failed"));
  QVERIFY(health.value(QStringLiteral("enabled")).toBool());
  QVERIFY(!health.value(QStringLiteral("prepared")).toBool());
  QVERIFY(!health.value(QStringLiteral("running")).toBool());
  QCOMPARE(health.value(QStringLiteral("hostError")).toInt(), ENETDOWN);
}

void SpatialAudioEngineTest::queuedFailureIsIgnoredAfterDisable() {
  LifecycleProbe probe;
  SpatialAudioEngine engine(hooksFor(probe));
  engine.start();
  engine.setEnabled(true);
  QTRY_COMPARE_WITH_TIMEOUT(engine.lifecycleStateName(),
                            QStringLiteral("running"), 3000);

  QVERIFY(writeRuntimeError(probe.notificationFds.back(), EPIPE));
  probe.writeDuringHostStop = true;
  engine.setEnabled(false);
  QTest::qWait(25);

  QCOMPARE(engine.lifecycleStateName(), QStringLiteral("disabled"));
  QVERIFY(!engine.enabled());
  QVERIFY(!engine.isPrepared());
  QVERIFY(!engine.isRunning());
  QCOMPARE(engine.hostError(), 0);
  QCOMPARE(probe.hostStopCalls.load(), 1);
  QCOMPARE(probe.destroyCalls.load(), 1);
  QVERIFY(probe.notificationFdOpenDuringHostStop);
  QCOMPARE(probe.stopWriteResult, static_cast<ssize_t>(sizeof(std::uint64_t)));
  QCOMPARE(probe.recordedEvents().size(), std::size_t(3));
}

void SpatialAudioEngineTest::queuedFailureIsIgnoredAfterRuntimeStop() {
  LifecycleProbe probe;
  SpatialAudioEngine engine(hooksFor(probe));
  engine.start();
  engine.setEnabled(true);
  QTRY_COMPARE_WITH_TIMEOUT(engine.lifecycleStateName(),
                            QStringLiteral("running"), 3000);

  QVERIFY(writeRuntimeError(probe.notificationFds.back(), ENETDOWN));
  probe.writeDuringHostStop = true;
  engine.stop();
  QTest::qWait(25);

  QVERIFY(engine.enabled());
  QVERIFY(!engine.isPrepared());
  QVERIFY(!engine.isRunning());
  QCOMPARE(engine.lifecycleStateName(), QStringLiteral("disabled"));
  QCOMPARE(engine.hostError(), 0);
  QCOMPARE(probe.hostStopCalls.load(), 1);
  QCOMPARE(probe.destroyCalls.load(), 1);
  QVERIFY(probe.notificationFdOpenDuringHostStop);
  QCOMPARE(probe.stopWriteResult, static_cast<ssize_t>(sizeof(std::uint64_t)));
}

void SpatialAudioEngineTest::staleFailureDoesNotStopTheNewActivation() {
  LifecycleProbe probe;
  SpatialAudioEngine engine(hooksFor(probe));
  engine.start();
  engine.setEnabled(true);
  QTRY_COMPARE_WITH_TIMEOUT(engine.lifecycleStateName(),
                            QStringLiteral("running"), 3000);
  const int oldNotificationFd = probe.notificationFds.back();
  QVERIFY(writeRuntimeError(oldNotificationFd, EPIPE));

  engine.setEnabled(false);
  engine.setEnabled(true);
  QTRY_COMPARE_WITH_TIMEOUT(engine.lifecycleStateName(),
                            QStringLiteral("running"), 3000);
  QCOMPARE(probe.notificationFds.size(), std::size_t(2));
  const int newNotificationFd = probe.notificationFds.back();
  QVERIFY(newNotificationFd >= 0);
  QTest::qWait(25);

  QCOMPARE(engine.lifecycleStateName(), QStringLiteral("running"));
  QVERIFY(engine.enabled());
  QVERIFY(engine.isPrepared());
  QVERIFY(engine.isRunning());
  QCOMPARE(engine.hostError(), 0);
  QCOMPARE(probe.hostStartCalls.load(), 2);
  QCOMPARE(probe.hostStopCalls.load(), 1);
  QCOMPARE(probe.destroyCalls.load(), 1);
}

void SpatialAudioEngineTest::destructionJoinsPreparationAndDestroysItsResult() {
  LifecycleProbe probe;
  probe.blockPreparation = true;
  auto engine = std::make_unique<SpatialAudioEngine>(hooksFor(probe));
  engine->start();
  engine->setEnabled(true);
  QTRY_VERIFY_WITH_TIMEOUT(
      probe.preparationStarted.load(std::memory_order_acquire), 3000);

  std::thread releaser([&probe] { probe.release(); });
  engine.reset();
  releaser.join();

  QCOMPARE(probe.hostStartCalls.load(), 0);
  QCOMPARE(probe.destroyCalls.load(), 1);
}

QTEST_GUILESS_MAIN(SpatialAudioEngineTest)
#include "SpatialAudioEngineTest.moc"
