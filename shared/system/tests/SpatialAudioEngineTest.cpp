#include "system/audio/spatial/SpatialAudioEngine.hpp"

#include <QTest>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
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
  hooks.startHost = [&probe](SpatialAudioHandle *handle, int *errorCode) {
    probe.record("start");
    ++probe.hostStartCalls;
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
  };
  return hooks;
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

  engine.setEnabled(false);
  QCOMPARE(probe.hostStopCalls.load(), 1);
  QCOMPARE(probe.destroyCalls.load(), 1);
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
