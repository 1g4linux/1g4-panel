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
};

void PipeWireEngineRestartTest::disconnectContextPurgesFakeRuntimeNodes() {
  PipeWireEngine engine;
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

QTEST_MAIN(PipeWireEngineRestartTest)
#include "pipewireengine_restart_test.moc"
