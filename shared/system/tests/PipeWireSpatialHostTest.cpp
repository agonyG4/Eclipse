#include "system/audio/spatial/PipeWireSpatialHost.hpp"
#include "system/audio/spatial/PipeWireSpatialHostSupport.hpp"

#include <pipewire/pipewire.h>
#include <pipewire/properties.h>
#include <spa/buffer/buffer.h>
#include <spa/param/audio/raw.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <QTest>

using namespace Astrea::System;
namespace Support = Astrea::System::PipeWireSpatialHostSupport;

namespace {

struct PlanarBuffer {
  explicit PlanarBuffer(std::size_t channels, std::size_t frames)
      : storage(channels), data(channels), chunks(channels) {
    buffer.n_datas = static_cast<std::uint32_t>(channels);
    buffer.datas = data.data();
    for (std::size_t channel = 0; channel < channels; ++channel) {
      storage[channel].resize(frames);
      data[channel].flags = SPA_DATA_FLAG_READABLE | SPA_DATA_FLAG_WRITABLE;
      data[channel].maxsize =
          static_cast<std::uint32_t>(storage[channel].size() * sizeof(float));
      data[channel].data = storage[channel].data();
      data[channel].chunk = &chunks[channel];
      chunks[channel].offset = 0;
      chunks[channel].size = data[channel].maxsize;
      chunks[channel].stride = sizeof(float);
    }
  }

  spa_buffer buffer{};
  std::vector<std::vector<float>> storage;
  std::vector<spa_data> data;
  std::vector<spa_chunk> chunks;
};

struct ProcessProbe {
  std::array<const float *, 8> expectedInputs{};
  std::array<float *, 2> expectedOutputs{};
  std::uint32_t frames = 0;
  std::uint32_t calls = 0;
  bool receivedCanonicalPlanes = false;
};

int probeProcessor(void *context, const float *const *inputs,
                   float *const *outputs, std::uint32_t frames) noexcept {
  auto &probe = *static_cast<ProcessProbe *>(context);
  ++probe.calls;
  probe.frames = frames;
  probe.receivedCanonicalPlanes = true;
  for (std::size_t channel = 0; channel < probe.expectedInputs.size();
       ++channel)
    probe.receivedCanonicalPlanes &=
        inputs[channel] == probe.expectedInputs[channel];
  for (std::size_t channel = 0; channel < probe.expectedOutputs.size();
       ++channel)
    probe.receivedCanonicalPlanes &=
        outputs[channel] == probe.expectedOutputs[channel];
  for (std::uint32_t frame = 0; frame < frames; ++frame) {
    outputs[0][frame] = inputs[0][frame] + inputs[7][frame];
    outputs[1][frame] = inputs[1][frame] + inputs[6][frame];
  }
  return 0;
}

struct PendingQueue {
  std::array<void *, 4> buffers{};
  std::size_t count = 0;
  std::size_t next = 0;
  std::array<void *, 4> recycled{};
  std::size_t recycleCount = 0;

  static void *dequeue(void *context) noexcept {
    auto &queue = *static_cast<PendingQueue *>(context);
    if (queue.next == queue.count)
      return nullptr;
    return queue.buffers[queue.next++];
  }

  static void queue(void *context, void *buffer) noexcept {
    auto &queue = *static_cast<PendingQueue *>(context);
    queue.recycled[queue.recycleCount++] = buffer;
  }

  Support::BufferQueue api() noexcept {
    return {this, &PendingQueue::dequeue, &PendingQueue::queue};
  }
};

QString pipewireProperty(const pw_properties *properties, const char *key) {
  const char *value = pw_properties_get(properties, key);
  return value ? QString::fromUtf8(value) : QString();
}

} // namespace

class PipeWireSpatialHostTest final : public QObject {
  Q_OBJECT

private slots:
  void canonicalMainFormatIsSevenPointOneFloatPlanar();
  void canonicalPlaybackFormatIsStereoFloatPlanar();
  void planarFloatSilenceSafetyRequiresExpectedChannelCount();
  void streamFlagsMatchPairedSchedulingContract();
  void mainPropertiesIdentifyVirtualAstreaSink();
  void bothStreamsShareTheInstanceLinkGroup();
  void playbackIsPassiveAndDoesNotRemix();
  void malformedCaptureProducesBoundedSilence();
  void insufficientOutputCapacityBoundsProcessing();
  void incompatiblePlaybackLayoutIsNotInterpretedAsFloat();
  void processorReceivesEightCanonicalInputPlanesAndTwoOutputs();
  void failedCaptureTriggerRecyclesAllPendingBuffers();
  void startupFailureAtEveryStageRollsBackOwnedResources();
  void repeatedStopIsSafe();
};

void PipeWireSpatialHostTest::canonicalMainFormatIsSevenPointOneFloatPlanar() {
  const auto format = Support::canonicalMainFormat();
  QCOMPARE(format.format, SPA_AUDIO_FORMAT_F32P);
  QCOMPARE(format.rate, 48'000U);
  QCOMPARE(format.channels, 8U);
  const std::array<std::uint32_t, 8> expected = {
      SPA_AUDIO_CHANNEL_FL,  SPA_AUDIO_CHANNEL_FR, SPA_AUDIO_CHANNEL_FC,
      SPA_AUDIO_CHANNEL_LFE, SPA_AUDIO_CHANNEL_RL, SPA_AUDIO_CHANNEL_RR,
      SPA_AUDIO_CHANNEL_SL,  SPA_AUDIO_CHANNEL_SR};
  for (std::size_t channel = 0; channel < expected.size(); ++channel)
    QCOMPARE(format.position[channel], expected[channel]);
}

void PipeWireSpatialHostTest::canonicalPlaybackFormatIsStereoFloatPlanar() {
  const auto format = Support::canonicalPlaybackFormat();
  QCOMPARE(format.format, SPA_AUDIO_FORMAT_F32P);
  QCOMPARE(format.rate, 48'000U);
  QCOMPARE(format.channels, 2U);
  QCOMPARE(format.position[0], SPA_AUDIO_CHANNEL_FL);
  QCOMPARE(format.position[1], SPA_AUDIO_CHANNEL_FR);
}

void PipeWireSpatialHostTest::
    planarFloatSilenceSafetyRequiresExpectedChannelCount() {
  auto format = Support::canonicalPlaybackFormat();
  QVERIFY(Support::isPlanarFloatAudio(format, 2));
  QVERIFY(!Support::isPlanarFloatAudio(format, 8));
  format.format = SPA_AUDIO_FORMAT_S16;
  QVERIFY(!Support::isPlanarFloatAudio(format, 2));
}

void PipeWireSpatialHostTest::streamFlagsMatchPairedSchedulingContract() {
  const auto captureFlags = static_cast<pw_stream_flags>(
      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
      PW_STREAM_FLAG_RT_PROCESS | PW_STREAM_FLAG_ASYNC);
  const auto playbackFlags = static_cast<pw_stream_flags>(
      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
      PW_STREAM_FLAG_RT_PROCESS | PW_STREAM_FLAG_TRIGGER);
  QCOMPARE(Support::captureStreamFlags(), captureFlags);
  QCOMPARE(Support::playbackStreamFlags(), playbackFlags);
}

void PipeWireSpatialHostTest::mainPropertiesIdentifyVirtualAstreaSink() {
  auto *properties = Support::makeMainProperties("astrea-spatial-test-1");
  QVERIFY(properties);
  QCOMPARE(pipewireProperty(properties, PW_KEY_MEDIA_CLASS),
           QStringLiteral("Audio/Sink"));
  QCOMPARE(pipewireProperty(properties, PW_KEY_NODE_VIRTUAL),
           QStringLiteral("true"));
  QCOMPARE(pipewireProperty(properties, PW_KEY_NODE_NAME),
           QStringLiteral("astrea_spatial_audio"));
  QCOMPARE(pipewireProperty(properties, PW_KEY_NODE_DESCRIPTION),
           QStringLiteral("Astrea Spatial Audio"));
  QCOMPARE(pipewireProperty(properties, PW_KEY_AUDIO_RATE),
           QStringLiteral("48000"));
  QCOMPARE(pipewireProperty(properties, PW_KEY_AUDIO_CHANNELS),
           QStringLiteral("8"));
  QCOMPARE(pipewireProperty(properties, SPA_KEY_AUDIO_POSITION),
           QStringLiteral("[ FL FR FC LFE RL RR SL SR ]"));
  QCOMPARE(pipewireProperty(properties, "filter.smart"),
           QStringLiteral("true"));
  QCOMPARE(pipewireProperty(properties, "filter.smart.name"),
           QStringLiteral("astrea-spatial-audio"));
  QCOMPARE(pipewireProperty(properties, "filter.smart.targetable"),
           QStringLiteral("true"));
  pw_properties_free(properties);
}

void PipeWireSpatialHostTest::bothStreamsShareTheInstanceLinkGroup() {
  constexpr auto group = "astrea-spatial-test-2";
  auto *main = Support::makeMainProperties(group);
  auto *playback = Support::makePlaybackProperties(group);
  QVERIFY(main);
  QVERIFY(playback);
  QCOMPARE(pipewireProperty(main, PW_KEY_NODE_GROUP),
           QString::fromLatin1(group));
  QCOMPARE(pipewireProperty(playback, PW_KEY_NODE_GROUP),
           QString::fromLatin1(group));
  QCOMPARE(pipewireProperty(main, PW_KEY_NODE_LINK_GROUP),
           QString::fromLatin1(group));
  QCOMPARE(pipewireProperty(playback, PW_KEY_NODE_LINK_GROUP),
           QString::fromLatin1(group));
  pw_properties_free(main);
  pw_properties_free(playback);
}

void PipeWireSpatialHostTest::playbackIsPassiveAndDoesNotRemix() {
  auto *properties = Support::makePlaybackProperties("astrea-spatial-test-3");
  QVERIFY(properties);
  QCOMPARE(pipewireProperty(properties, PW_KEY_MEDIA_CLASS),
           QStringLiteral("Stream/Output/Audio"));
  QCOMPARE(pipewireProperty(properties, PW_KEY_NODE_PASSIVE),
           QStringLiteral("true"));
  QCOMPARE(pipewireProperty(properties, PW_KEY_STREAM_DONT_REMIX),
           QStringLiteral("true"));
  QCOMPARE(pipewireProperty(properties, PW_KEY_AUDIO_RATE),
           QStringLiteral("48000"));
  QCOMPARE(pipewireProperty(properties, PW_KEY_AUDIO_CHANNELS),
           QStringLiteral("2"));
  QCOMPARE(pipewireProperty(properties, SPA_KEY_AUDIO_POSITION),
           QStringLiteral("[ FL FR ]"));
  pw_properties_free(properties);
}

void PipeWireSpatialHostTest::malformedCaptureProducesBoundedSilence() {
  PlanarBuffer input(8, 8);
  PlanarBuffer output(2, 4);
  ProcessProbe probe;
  std::fill(output.storage[0].begin(), output.storage[0].end(), 4.0f);
  std::fill(output.storage[1].begin(), output.storage[1].end(), 5.0f);

  input.buffer.n_datas = 7;
  const auto result = Support::processBufferPair(
      &input.buffer, &output.buffer, 8, true, true, &probeProcessor, &probe);
  QCOMPARE(result.frames, 4U);
  QVERIFY(!result.processed);
  QCOMPARE(probe.calls, 0U);
  for (std::size_t frame = 0; frame < 4; ++frame) {
    QCOMPARE(output.storage[0][frame], 0.0f);
    QCOMPARE(output.storage[1][frame], 0.0f);
  }
  QCOMPARE(output.chunks[0].size, 4U * sizeof(float));
  QCOMPARE(output.chunks[1].size, 4U * sizeof(float));

  input.buffer.n_datas = 8;
  input.data[3].data = nullptr;
  std::fill(output.storage[0].begin(), output.storage[0].end(), 9.0f);
  std::fill(output.storage[1].begin(), output.storage[1].end(), 9.0f);
  const auto missing = Support::processBufferPair(
      &input.buffer, &output.buffer, 2, true, true, &probeProcessor, &probe);
  QCOMPARE(missing.frames, 2U);
  QVERIFY(!missing.processed);
  for (std::size_t frame = 0; frame < 2; ++frame) {
    QCOMPARE(output.storage[0][frame], 0.0f);
    QCOMPARE(output.storage[1][frame], 0.0f);
  }
  QCOMPARE(probe.calls, 0U);

  input.data[3].data = input.storage[3].data();
  input.chunks[5].size = sizeof(float);
  output.storage[0][0] = 8.0f;
  output.storage[0][1] = 8.0f;
  output.storage[1][0] = 8.0f;
  output.storage[1][1] = 8.0f;
  const auto shortPlane = Support::processBufferPair(
      &input.buffer, &output.buffer, 2, true, true, &probeProcessor, &probe);
  QCOMPARE(shortPlane.frames, 2U);
  QVERIFY(!shortPlane.processed);
  for (std::size_t channel = 0; channel < 2; ++channel)
    for (std::size_t frame = 0; frame < 2; ++frame)
      QCOMPARE(output.storage[channel][frame], 0.0f);
  QCOMPARE(probe.calls, 0U);

  input.chunks[5].size = input.data[5].maxsize;
  std::fill(output.storage[0].begin(), output.storage[0].end(), 3.0f);
  std::fill(output.storage[1].begin(), output.storage[1].end(), 4.0f);
  const auto unsupportedFormat = Support::processBufferPair(
      &input.buffer, &output.buffer, 2, false, true, &probeProcessor, &probe);
  QCOMPARE(unsupportedFormat.frames, 2U);
  QVERIFY(!unsupportedFormat.processed);
  QCOMPARE(output.storage[0][0], 0.0f);
  QCOMPARE(output.storage[0][1], 0.0f);
  QCOMPARE(output.storage[1][0], 0.0f);
  QCOMPARE(output.storage[1][1], 0.0f);
  QCOMPARE(probe.calls, 0U);
}

void PipeWireSpatialHostTest::insufficientOutputCapacityBoundsProcessing() {
  PlanarBuffer input(8, 8);
  PlanarBuffer output(2, 8);
  for (std::size_t channel = 0; channel < input.storage.size(); ++channel)
    for (std::size_t frame = 0; frame < input.storage[channel].size(); ++frame)
      input.storage[channel][frame] = static_cast<float>(channel * 10 + frame);
  output.data[0].maxsize = 2U * sizeof(float);
  output.data[1].maxsize = 3U * sizeof(float);
  ProcessProbe probe;

  const auto result = Support::processBufferPair(
      &input.buffer, &output.buffer, 7, true, true, &probeProcessor, &probe);
  QVERIFY(result.processed);
  QCOMPARE(result.frames, 2U);
  QCOMPARE(probe.calls, 1U);
  QCOMPARE(probe.frames, 2U);
  QCOMPARE(output.chunks[0].size, 2U * sizeof(float));
  QCOMPARE(output.chunks[1].size, 2U * sizeof(float));
  QCOMPARE(output.chunks[0].stride, static_cast<std::int32_t>(sizeof(float)));
  QCOMPARE(output.chunks[1].stride, static_cast<std::int32_t>(sizeof(float)));
  QCOMPARE(output.storage[0][1], 72.0f);
  QCOMPARE(output.storage[1][1], 72.0f);
}

void PipeWireSpatialHostTest::
    incompatiblePlaybackLayoutIsNotInterpretedAsFloat() {
  PlanarBuffer input(8, 4);
  PlanarBuffer output(2, 4);
  for (auto &plane : output.storage)
    std::fill(plane.begin(), plane.end(), 8.0f);
  ProcessProbe probe;

  const auto result =
      Support::processBufferPair(&input.buffer, &output.buffer, 4, true, false,
                                 &probeProcessor, &probe, false);
  QCOMPARE(result.frames, 0U);
  QVERIFY(!result.processed);
  QCOMPARE(probe.calls, 0U);
  for (const auto &plane : output.storage)
    for (const auto sample : plane)
      QCOMPARE(sample, 8.0f);
  QCOMPARE(output.chunks[0].size, output.data[0].maxsize);
  QCOMPARE(output.chunks[1].size, output.data[1].maxsize);

  for (auto &plane : output.storage)
    std::fill(plane.begin(), plane.end(), 8.0f);
  const auto safeSilence =
      Support::processBufferPair(&input.buffer, &output.buffer, 4, true, false,
                                 &probeProcessor, &probe, true);
  QCOMPARE(safeSilence.frames, 4U);
  QVERIFY(!safeSilence.processed);
  for (const auto &plane : output.storage)
    for (const auto sample : plane)
      QCOMPARE(sample, 0.0f);
}

void PipeWireSpatialHostTest::
    processorReceivesEightCanonicalInputPlanesAndTwoOutputs() {
  PlanarBuffer input(8, 6);
  PlanarBuffer output(2, 6);
  for (std::size_t channel = 0; channel < input.storage.size(); ++channel) {
    for (std::size_t frame = 0; frame < input.storage[channel].size(); ++frame)
      input.storage[channel][frame] = static_cast<float>(channel * 10 + frame);
    input.chunks[channel].offset = sizeof(float);
    input.chunks[channel].size = 3U * sizeof(float);
  }
  ProcessProbe probe;
  for (std::size_t channel = 0; channel < input.storage.size(); ++channel)
    probe.expectedInputs[channel] = input.storage[channel].data() + 1;
  probe.expectedOutputs = {output.storage[0].data(), output.storage[1].data()};

  const auto result = Support::processBufferPair(
      &input.buffer, &output.buffer, 3, true, true, &probeProcessor, &probe);
  QVERIFY(result.processed);
  QCOMPARE(result.frames, 3U);
  QCOMPARE(probe.calls, 1U);
  QCOMPARE(probe.frames, 3U);
  QVERIFY(probe.receivedCanonicalPlanes);
  QCOMPARE(output.storage[0][0], 72.0f);
  QCOMPARE(output.storage[1][2], 76.0f);
  QCOMPARE(output.chunks[0].offset, 0U);
  QCOMPARE(output.chunks[1].offset, 0U);
  QCOMPARE(output.chunks[0].size, 3U * sizeof(float));
  QCOMPARE(output.chunks[1].size, 3U * sizeof(float));
}

void PipeWireSpatialHostTest::failedCaptureTriggerRecyclesAllPendingBuffers() {
  int a = 1;
  int b = 2;
  int c = 3;
  PendingQueue queue{{&a, &b, &c, nullptr}, 3};

  Support::recyclePendingBuffers(queue.api());
  QCOMPARE(queue.next, 3U);
  QCOMPARE(queue.recycleCount, 3U);
  QCOMPARE(queue.recycled[0], static_cast<void *>(&a));
  QCOMPARE(queue.recycled[1], static_cast<void *>(&b));
  QCOMPARE(queue.recycled[2], static_cast<void *>(&c));

  PendingQueue newest{{&a, &b, &c, nullptr}, 3};
  void *current = Support::dequeueNewestBuffer(newest.api());
  QCOMPARE(current, static_cast<void *>(&c));
  QCOMPARE(newest.next, 3U);
  QCOMPARE(newest.recycleCount, 2U);
  QCOMPARE(newest.recycled[0], static_cast<void *>(&a));
  QCOMPARE(newest.recycled[1], static_cast<void *>(&b));
}

void PipeWireSpatialHostTest::
    startupFailureAtEveryStageRollsBackOwnedResources() {
  for (const auto failedStage : Support::StartupStages) {
    std::vector<Support::StartupStage> owned;
    std::uint32_t rollbacks = 0;
    Support::StartupStage observedFailure =
        Support::StartupStage::ThreadLoopCreate;
    const bool started = Support::runStartupTransaction(
        [&](Support::StartupStage stage) {
          if (stage == failedStage)
            return false;
          owned.push_back(stage);
          return true;
        },
        [&] {
          ++rollbacks;
          owned.clear();
        },
        &observedFailure);

    QVERIFY(!started);
    QCOMPARE(observedFailure, failedStage);
    QVERIFY(owned.empty());
    QCOMPARE(rollbacks, 1U);
  }
}

void PipeWireSpatialHostTest::repeatedStopIsSafe() {
  PipeWireSpatialHost host;
  host.stop();
  host.stop();
  QVERIFY(!host.isRunning());
}

QTEST_GUILESS_MAIN(PipeWireSpatialHostTest)
#include "PipeWireSpatialHostTest.moc"
