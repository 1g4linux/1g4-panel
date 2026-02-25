#define private public
#include "pipewireengine.h"
#undef private

#include <QSignalSpy>
#include <QtTest/QtTest>

#include <algorithm>

class PipeWireEngineRestartTest : public QObject {
  Q_OBJECT

 private slots:
  void disconnectContextPurgesFakeRuntimeNodes();
  void metadataDefaultNodeUpdatesAffectStateSnapshot();
};

void PipeWireEngineRestartTest::disconnectContextPurgesFakeRuntimeNodes() {
  PipeWireEngine engine(PipeWireEngine::RuntimeMode::DisabledForTests);
  engine.m_reconnectionTimer.stop();

  QSignalSpy sinkListChangedSpy(&engine, &AudioEngine::sinkListChanged);

  constexpr uint32_t kFakeSinkId = 4'000'000'001U;
  constexpr uint32_t kFakeSourceId = 4'000'000'002U;

  engine.addOrUpdateNode(kFakeSinkId, QStringLiteral("alsa_output.test_sink"), QStringLiteral("Test Sink"), Sink, 11,
                         QStringLiteral("analog-stereo"));
  engine.addOrUpdateNode(kFakeSourceId, QStringLiteral("alsa_input.test_source"), QStringLiteral("Test Source"), Source,
                         11, QStringLiteral("analog-stereo"));

  QVERIFY(engine.m_deviceByWpId.contains(kFakeSinkId));
  QVERIFY(engine.m_deviceByWpId.contains(kFakeSourceId));
  QVERIFY(engine.m_nodeIdByDevice.values().contains(kFakeSinkId));
  QVERIFY(engine.m_nodeIdByDevice.values().contains(kFakeSourceId));

  const int sinkSignalsBeforeDisconnect = sinkListChangedSpy.count();
  engine.disconnectContext();
  engine.m_reconnectionTimer.stop();

  QVERIFY(!engine.m_deviceByWpId.contains(kFakeSinkId));
  QVERIFY(!engine.m_deviceByWpId.contains(kFakeSourceId));
  QVERIFY(!engine.m_nodeIdByDevice.values().contains(kFakeSinkId));
  QVERIFY(!engine.m_nodeIdByDevice.values().contains(kFakeSourceId));
  QVERIFY(sinkListChangedSpy.count() > sinkSignalsBeforeDisconnect);

  const AudioEngine::StateSnapshot snapshot = engine.stateSnapshot();
  const bool staleEndpointStillPresent =
      std::any_of(snapshot.logicalEndpoints.begin(), snapshot.logicalEndpoints.end(),
                  [](const AudioEngine::LogicalEndpointSnapshot& endpoint) {
                    return endpoint.runtimeId == kFakeSinkId || endpoint.runtimeId == kFakeSourceId;
                  });
  QVERIFY(!staleEndpointStillPresent);
}

void PipeWireEngineRestartTest::metadataDefaultNodeUpdatesAffectStateSnapshot() {
  PipeWireEngine engine(PipeWireEngine::RuntimeMode::DisabledForTests);
  engine.m_reconnectionTimer.stop();

  constexpr uint32_t kFakeSinkId = 4'000'000'003U;
  constexpr uint32_t kFakeSourceId = 4'000'000'004U;

  engine.addOrUpdateNode(kFakeSinkId, QStringLiteral("alsa_output.default_sink"), QStringLiteral("Default Sink"), Sink,
                         12, QStringLiteral("analog-stereo"));
  engine.addOrUpdateNode(kFakeSourceId, QStringLiteral("alsa_input.default_source"),
                         QStringLiteral("Default Source"), Source, 12, QStringLiteral("analog-stereo"));

  PipeWireEngine::onMetadataProperty(&engine, 0U, "default.audio.sink", "Spa:String",
                                     "{\"name\":\"alsa_output.default_sink\"}");
  PipeWireEngine::onMetadataProperty(&engine, 0U, "default.audio.source", "Spa:String",
                                     "{\"name\":\"alsa_input.default_source\"}");
  QCoreApplication::processEvents();

  AudioEngine::StateSnapshot state = engine.stateSnapshot();

  bool sinkIsDefault = false;
  bool sourceIsDefault = false;
  for (const AudioEngine::LogicalEndpointSnapshot& endpoint : std::as_const(state.logicalEndpoints)) {
    if (endpoint.runtimeId == kFakeSinkId) {
      sinkIsDefault = endpoint.isDefault;
    }
    else if (endpoint.runtimeId == kFakeSourceId) {
      sourceIsDefault = endpoint.isDefault;
    }
  }
  QVERIFY(sinkIsDefault);
  QVERIFY(sourceIsDefault);

  PipeWireEngine::onMetadataProperty(&engine, 0U, "default.audio.sink", "Spa:String", nullptr);
  QCoreApplication::processEvents();
  state = engine.stateSnapshot();

  for (const AudioEngine::LogicalEndpointSnapshot& endpoint : std::as_const(state.logicalEndpoints)) {
    if (endpoint.runtimeId == kFakeSinkId) {
      QCOMPARE(endpoint.isDefault, false);
    }
  }
}

QTEST_MAIN(PipeWireEngineRestartTest)
#include "pipewireengine_restart_test.moc"
