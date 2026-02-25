#include "audiodevice.h"
#include "audioengine.h"

#include <QtTest/QtTest>

class SnapshotDummyEngine : public AudioEngine {
  Q_OBJECT

 public:
  explicit SnapshotDummyEngine(QObject* parent = nullptr)
      : AudioEngine(parent),
        m_capabilities{
            false,
            false,
            false,
            false,
        },
        m_commitVolumeSucceeds(true),
        m_commitMuteSucceeds(true),
        m_autoAcknowledgeVolumeCommit(false),
        m_autoAcknowledgeMuteCommit(false) {}

  int volumeMax(AudioDevice* /*device*/) const override {
    return 100;
  }

  const QString backendName() const override {
    return QStringLiteral("snapshot-dummy");
  }

  BackendCapabilities backendCapabilities() const override {
    return m_capabilities;
  }

  bool commitDeviceVolume(AudioDevice* device) override {
    if (!m_commitVolumeSucceeds || !device) {
      return false;
    }
    if (m_autoAcknowledgeVolumeCommit) {
      device->setVolumeNoCommit(device->volume());
    }
    return true;
  }

  bool setMute(AudioDevice* device, bool state) override {
    if (!m_commitMuteSucceeds || !device) {
      return false;
    }
    if (m_autoAcknowledgeMuteCommit) {
      device->setMuteNoCommit(state);
    }
    return true;
  }

  AudioDevice* addSink(const QString& name,
                       const QString& description,
                       uint runtimeId,
                       int volume,
                       bool mute,
                       int cardId) {
    auto* sink = new AudioDevice(Sink, this, this);
    sink->setName(name);
    sink->setDescription(description);
    sink->setIndex(runtimeId);
    sink->setVolumeNoCommit(volume);
    sink->setMuteNoCommit(mute);
    sink->setCardId(cardId);
    m_sinks.append(sink);
    return sink;
  }

  void setCapabilities(bool profileSwitching, bool streamMove, bool battery, bool perPortSelection) {
    m_capabilities = BackendCapabilities{
        profileSwitching,
        streamMove,
        battery,
        perPortSelection,
    };
  }

  void setCommitBehavior(bool volumeSucceeds, bool muteSucceeds) {
    m_commitVolumeSucceeds = volumeSucceeds;
    m_commitMuteSucceeds = muteSucceeds;
  }

  void setAutoAcknowledge(bool volumeAutoAck, bool muteAutoAck) {
    m_autoAcknowledgeVolumeCommit = volumeAutoAck;
    m_autoAcknowledgeMuteCommit = muteAutoAck;
  }

  void removeSink(AudioDevice* sink) {
    if (!sink) {
      return;
    }
    m_sinks.removeAll(sink);
    delete sink;
    emit sinkListChanged();
  }

  void emitSinkListChangedForTest() {
    emit sinkListChanged();
  }

  void setBackendHealthForTest(BackendHealthState state, const QString& message) {
    setBackendHealth(state, message);
  }

  void recordReconnectAttemptForTest(const QString& message) {
    recordReconnectAttempt(message);
  }

 private:
  BackendCapabilities m_capabilities;
  bool m_commitVolumeSucceeds;
  bool m_commitMuteSucceeds;
  bool m_autoAcknowledgeVolumeCommit;
  bool m_autoAcknowledgeMuteCommit;
};

class AudioEngineSnapshotTest : public QObject {
  Q_OBJECT

 private slots:
  void snapshotIsDetachedFromMutableDeviceState();
  void stableIdDoesNotDependOnRuntimeId();
  void stateSnapshotSeparatesPhysicalDevicesLogicalEndpointsAndStreams();
  void stateSnapshotIncludesBackendCapabilitiesFlags();
  void userVolumeChangeCreatesPendingOperationUntilBackendAcknowledges();
  void failedUserVolumeChangeRollsBackToPreviousValue();
  void backendUpdateMarksEndpointChangeSource();
  void backendControlInterfacesDefaultToUnsupported();
  void stateChangedSignalSubscribesToDiscoveryUpdates();
  void stateChangedSignalCoalescesDiscoveryBursts();
  void stateChangedSignalCoalescesEndpointBurstByKeyAndType();
  void stateChangedSignalCoalescesBackendHealthBurstByKeyAndType();
  void stateSnapshotTracksCoalescerMetrics();
  void volumeCommitRequestRunsAsynchronously();
  void rapidVolumeBurstFailureRollsBackToPreBurstValue();
  void rapidMuteBurstFailureRollsBackToPreBurstValue();
  void stateSnapshotTracksBackendHealthAndReconnectAttempts();
  void removingEndpointClearsStalePendingOperations();
  void panelUnloadQueuedVolumeCommitSafeDuringEngineDestruction();
  void sessionLogoutQueuedMuteCommitSafeDuringEngineDestruction();
  void backendCrashQueuedStateFlushSafeDuringEngineDestruction();
};

void AudioEngineSnapshotTest::snapshotIsDetachedFromMutableDeviceState() {
  SnapshotDummyEngine engine;
  AudioDevice* sink =
      engine.addSink(QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo"), QStringLiteral("Built-in Audio"),
                     41U, 35, false, 19);

  const QList<AudioEngine::SinkSnapshot> snapshots = engine.sinkSnapshots();
  QCOMPARE(snapshots.size(), 1);
  const AudioEngine::SinkSnapshot first = snapshots.first();

  sink->setIndex(77U);
  sink->setDescription(QStringLiteral("Changed Description"));
  sink->setVolumeNoCommit(88);
  sink->setMuteNoCommit(true);

  QCOMPARE(first.runtimeId, 41U);
  QCOMPARE(first.description, QStringLiteral("Built-in Audio"));
  QCOMPARE(first.volumePercent, 35);
  QCOMPARE(first.muted, false);
}

void AudioEngineSnapshotTest::stableIdDoesNotDependOnRuntimeId() {
  SnapshotDummyEngine engine;
  AudioDevice* sink =
      engine.addSink(QStringLiteral("bluez_output.00_11_22_33_44_55.a2dp-sink"), QStringLiteral("BT Headset"), 8U, 64,
                     false, -1);

  const QList<AudioEngine::SinkSnapshot> first = engine.sinkSnapshots();
  QCOMPARE(first.size(), 1);

  const QString stableIdBefore = first.first().stableId;
  QVERIFY(!stableIdBefore.isEmpty());
  QCOMPARE(first.first().runtimeId, 8U);

  sink->setIndex(102U);

  const QList<AudioEngine::SinkSnapshot> second = engine.sinkSnapshots();
  QCOMPARE(second.size(), 1);
  QCOMPARE(second.first().runtimeId, 102U);
  QCOMPARE(second.first().stableId, stableIdBefore);
}

void AudioEngineSnapshotTest::stateSnapshotSeparatesPhysicalDevicesLogicalEndpointsAndStreams() {
  SnapshotDummyEngine engine;
  engine.addSink(QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo"), QStringLiteral("Built-in Analog"), 10U,
                 20, false, 7);
  engine.addSink(QStringLiteral("alsa_output.pci-0000_00_1f.3.hdmi-stereo"), QStringLiteral("Built-in HDMI"), 11U, 70,
                 true, 7);

  const AudioEngine::StateSnapshot state = engine.stateSnapshot();

  QCOMPARE(state.physicalDevices.size(), 1);
  QCOMPARE(state.logicalEndpoints.size(), 2);
  QVERIFY(state.streams.isEmpty());

  const QString physicalStableId = state.physicalDevices.first().stableId;
  QVERIFY(!physicalStableId.isEmpty());
  QCOMPARE(state.physicalDevices.first().cardId, 7);

  for (const AudioEngine::LogicalEndpointSnapshot& endpoint : state.logicalEndpoints) {
    QCOMPARE(endpoint.physicalDeviceStableId, physicalStableId);
    QCOMPARE(endpoint.direction, AudioEngine::EndpointDirection::Output);
  }
}

void AudioEngineSnapshotTest::stateSnapshotIncludesBackendCapabilitiesFlags() {
  SnapshotDummyEngine engine;
  engine.setCapabilities(true, false, true, false);

  const AudioEngine::StateSnapshot state = engine.stateSnapshot();
  QCOMPARE(state.capabilities.profileSwitchingSupported, true);
  QCOMPARE(state.capabilities.streamMoveSupported, false);
  QCOMPARE(state.capabilities.batteryAvailable, true);
  QCOMPARE(state.capabilities.perPortSelectionSupported, false);
}

void AudioEngineSnapshotTest::userVolumeChangeCreatesPendingOperationUntilBackendAcknowledges() {
  SnapshotDummyEngine engine;
  engine.setCommitBehavior(true, true);
  engine.setAutoAcknowledge(false, false);
  AudioDevice* sink = engine.addSink(QStringLiteral("alsa_output.test"), QStringLiteral("Test Sink"), 1U, 20, false, 1);

  sink->setVolume(63);

  AudioEngine::StateSnapshot state = engine.stateSnapshot();
  QCOMPARE(state.pendingOperations.size(), 1);
  const AudioEngine::PendingOperationSnapshot pending = state.pendingOperations.first();
  QCOMPARE(pending.kind, AudioEngine::PendingOperationKind::SetVolume);
  QCOMPARE(pending.previousVolumePercent, 20);
  QCOMPARE(pending.targetVolumePercent, 63);

  QCOMPARE(state.logicalEndpoints.size(), 1);
  QCOMPARE(state.logicalEndpoints.first().lastChangeSource, AudioEngine::ChangeSource::UserAction);

  sink->setVolumeNoCommit(63);

  state = engine.stateSnapshot();
  QVERIFY(state.pendingOperations.isEmpty());
}

void AudioEngineSnapshotTest::failedUserVolumeChangeRollsBackToPreviousValue() {
  SnapshotDummyEngine engine;
  engine.setCommitBehavior(false, true);
  AudioDevice* sink = engine.addSink(QStringLiteral("alsa_output.test"), QStringLiteral("Test Sink"), 2U, 15, false, 1);

  sink->setVolume(80);
  QCoreApplication::processEvents();

  QCOMPARE(sink->volume(), 15);
  const AudioEngine::StateSnapshot state = engine.stateSnapshot();
  QVERIFY(state.pendingOperations.isEmpty());
  QCOMPARE(state.logicalEndpoints.size(), 1);
  QCOMPARE(state.logicalEndpoints.first().lastChangeSource, AudioEngine::ChangeSource::Rollback);
}

void AudioEngineSnapshotTest::backendUpdateMarksEndpointChangeSource() {
  SnapshotDummyEngine engine;
  AudioDevice* sink = engine.addSink(QStringLiteral("alsa_output.test"), QStringLiteral("Test Sink"), 3U, 35, false, 1);

  sink->setVolumeNoCommit(44);

  const AudioEngine::StateSnapshot state = engine.stateSnapshot();
  QCOMPARE(state.logicalEndpoints.size(), 1);
  QCOMPARE(state.logicalEndpoints.first().volumePercent, 44);
  QCOMPARE(state.logicalEndpoints.first().lastChangeSource, AudioEngine::ChangeSource::BackendEvent);
}

void AudioEngineSnapshotTest::backendControlInterfacesDefaultToUnsupported() {
  SnapshotDummyEngine engine;

  QVERIFY(!engine.setDefaultOutputDevice(QStringLiteral("endpoint-a")));
  QVERIFY(!engine.setDefaultInputDevice(QStringLiteral("endpoint-b")));
  QVERIFY(!engine.movePlaybackStreamToOutput(QStringLiteral("stream-1"), QStringLiteral("endpoint-a")));
  QVERIFY(!engine.moveRecordingStreamToInput(QStringLiteral("stream-2"), QStringLiteral("endpoint-b")));
  QVERIFY(!engine.setPhysicalDeviceProfile(QStringLiteral("physical-1"), QStringLiteral("a2dp")));
  QVERIFY(engine.persistenceHints().isEmpty());
}

void AudioEngineSnapshotTest::stateChangedSignalSubscribesToDiscoveryUpdates() {
  SnapshotDummyEngine engine;
  QSignalSpy stateChangedSpy(&engine, &AudioEngine::stateChanged);

  engine.emitSinkListChangedForTest();

  QTRY_COMPARE_WITH_TIMEOUT(stateChangedSpy.count(), 1, 200);
}

void AudioEngineSnapshotTest::stateChangedSignalCoalescesDiscoveryBursts() {
  SnapshotDummyEngine engine;
  QSignalSpy stateChangedSpy(&engine, &AudioEngine::stateChanged);

  engine.emitSinkListChangedForTest();
  engine.emitSinkListChangedForTest();
  engine.emitSinkListChangedForTest();

  QTRY_COMPARE_WITH_TIMEOUT(stateChangedSpy.count(), 1, 200);
  QTest::qWait(50);
  QCOMPARE(stateChangedSpy.count(), 1);

  engine.emitSinkListChangedForTest();
  engine.emitSinkListChangedForTest();
  QTRY_COMPARE_WITH_TIMEOUT(stateChangedSpy.count(), 2, 200);
}

void AudioEngineSnapshotTest::stateChangedSignalCoalescesEndpointBurstByKeyAndType() {
  SnapshotDummyEngine engine;
  engine.setCommitBehavior(true, true);
  engine.setAutoAcknowledge(false, false);
  AudioDevice* sink =
      engine.addSink(QStringLiteral("alsa_output.coalesce"), QStringLiteral("Coalesce Sink"), 21U, 35, false, 2);
  QVERIFY(sink != nullptr);

  QSignalSpy stateChangedSpy(&engine, &AudioEngine::stateChanged);

  sink->setVolume(70);

  QTRY_COMPARE_WITH_TIMEOUT(stateChangedSpy.count(), 1, 200);
  QTest::qWait(50);
  QCOMPARE(stateChangedSpy.count(), 1);
}

void AudioEngineSnapshotTest::stateChangedSignalCoalescesBackendHealthBurstByKeyAndType() {
  SnapshotDummyEngine engine;
  QSignalSpy stateChangedSpy(&engine, &AudioEngine::stateChanged);

  engine.setBackendHealthForTest(AudioEngine::BackendHealthState::Reconnecting, QStringLiteral("first"));
  engine.setBackendHealthForTest(AudioEngine::BackendHealthState::Reconnecting, QStringLiteral("second"));

  QTRY_COMPARE_WITH_TIMEOUT(stateChangedSpy.count(), 1, 200);
  QTest::qWait(50);
  QCOMPARE(stateChangedSpy.count(), 1);

  const AudioEngine::StateSnapshot state = engine.stateSnapshot();
  QCOMPARE(state.backendHealth.state, AudioEngine::BackendHealthState::Reconnecting);
  QCOMPARE(state.backendHealth.message, QStringLiteral("second"));
}

void AudioEngineSnapshotTest::stateSnapshotTracksCoalescerMetrics() {
  SnapshotDummyEngine engine;
  QSignalSpy stateChangedSpy(&engine, &AudioEngine::stateChanged);

  AudioEngine::StateSnapshot state = engine.stateSnapshot();
  QCOMPARE(state.coalescerMetrics.enqueueRequests, quint64(0));
  QCOMPARE(state.coalescerMetrics.uniqueQueuedEvents, quint64(0));
  QCOMPARE(state.coalescerMetrics.duplicateEvents, quint64(0));
  QCOMPARE(state.coalescerMetrics.flushedEventCount, quint64(0));
  QCOMPARE(state.coalescerMetrics.stateChangedEmits, quint64(0));

  engine.emitSinkListChangedForTest();
  QTRY_COMPARE_WITH_TIMEOUT(stateChangedSpy.count(), 1, 200);

  state = engine.stateSnapshot();
  QCOMPARE(state.coalescerMetrics.enqueueRequests, quint64(1));
  QCOMPARE(state.coalescerMetrics.uniqueQueuedEvents, quint64(1));
  QCOMPARE(state.coalescerMetrics.duplicateEvents, quint64(0));
  QCOMPARE(state.coalescerMetrics.flushedEventCount, quint64(1));
  QCOMPARE(state.coalescerMetrics.stateChangedEmits, quint64(1));

  engine.emitSinkListChangedForTest();
  engine.emitSinkListChangedForTest();
  QTRY_COMPARE_WITH_TIMEOUT(stateChangedSpy.count(), 2, 200);

  state = engine.stateSnapshot();
  QCOMPARE(state.coalescerMetrics.enqueueRequests, quint64(3));
  QCOMPARE(state.coalescerMetrics.uniqueQueuedEvents, quint64(2));
  QCOMPARE(state.coalescerMetrics.duplicateEvents, quint64(1));
  QCOMPARE(state.coalescerMetrics.flushedEventCount, quint64(2));
  QCOMPARE(state.coalescerMetrics.stateChangedEmits, quint64(2));
}

void AudioEngineSnapshotTest::volumeCommitRequestRunsAsynchronously() {
  SnapshotDummyEngine engine;
  engine.setCommitBehavior(false, true);
  AudioDevice* sink =
      engine.addSink(QStringLiteral("alsa_output.async"), QStringLiteral("Async Sink"), 4U, 25, false, 1);

  sink->setVolume(77);

  QCOMPARE(sink->volume(), 77);
  QCOMPARE(engine.stateSnapshot().pendingOperations.size(), 1);

  QCoreApplication::processEvents();

  QCOMPARE(sink->volume(), 25);
  QVERIFY(engine.stateSnapshot().pendingOperations.isEmpty());
}

void AudioEngineSnapshotTest::rapidVolumeBurstFailureRollsBackToPreBurstValue() {
  SnapshotDummyEngine engine;
  engine.setCommitBehavior(false, true);
  AudioDevice* sink =
      engine.addSink(QStringLiteral("alsa_output.burst-volume"), QStringLiteral("Burst Volume Sink"), 6U, 20, false, 1);

  sink->setVolume(35);
  sink->setVolume(80);

  QCoreApplication::processEvents();

  QCOMPARE(sink->volume(), 20);
  const AudioEngine::StateSnapshot state = engine.stateSnapshot();
  QVERIFY(state.pendingOperations.isEmpty());
  QCOMPARE(state.logicalEndpoints.size(), 1);
  QCOMPARE(state.logicalEndpoints.first().lastChangeSource, AudioEngine::ChangeSource::Rollback);
}

void AudioEngineSnapshotTest::rapidMuteBurstFailureRollsBackToPreBurstValue() {
  SnapshotDummyEngine engine;
  engine.setCommitBehavior(true, false);
  AudioDevice* sink =
      engine.addSink(QStringLiteral("alsa_output.burst-mute"), QStringLiteral("Burst Mute Sink"), 7U, 45, false, 1);

  sink->setMute(true);
  sink->setMute(false);

  QCoreApplication::processEvents();

  QCOMPARE(sink->mute(), false);
  const AudioEngine::StateSnapshot state = engine.stateSnapshot();
  QVERIFY(state.pendingOperations.isEmpty());
  QCOMPARE(state.logicalEndpoints.size(), 1);
  QCOMPARE(state.logicalEndpoints.first().lastChangeSource, AudioEngine::ChangeSource::Rollback);
}

void AudioEngineSnapshotTest::stateSnapshotTracksBackendHealthAndReconnectAttempts() {
  SnapshotDummyEngine engine;

  engine.setBackendHealthForTest(AudioEngine::BackendHealthState::Reconnecting, QStringLiteral("initial reconnect"));
  engine.recordReconnectAttemptForTest(QStringLiteral("retry #1"));
  engine.recordReconnectAttemptForTest(QStringLiteral("retry #2"));

  AudioEngine::StateSnapshot state = engine.stateSnapshot();
  QCOMPARE(state.backendHealth.state, AudioEngine::BackendHealthState::Reconnecting);
  QCOMPARE(state.backendHealth.reconnectAttempts, 2U);
  QCOMPARE(state.backendHealth.message, QStringLiteral("retry #2"));

  engine.setBackendHealthForTest(AudioEngine::BackendHealthState::Ready, QString());
  state = engine.stateSnapshot();
  QCOMPARE(state.backendHealth.state, AudioEngine::BackendHealthState::Ready);
  QCOMPARE(state.backendHealth.reconnectAttempts, 2U);
}

void AudioEngineSnapshotTest::removingEndpointClearsStalePendingOperations() {
  SnapshotDummyEngine engine;
  engine.setCommitBehavior(true, true);
  engine.setAutoAcknowledge(false, false);
  AudioDevice* sink =
      engine.addSink(QStringLiteral("alsa_output.stale"), QStringLiteral("Stale Sink"), 5U, 30, false, 1);

  sink->setVolume(65);
  QCOMPARE(engine.stateSnapshot().pendingOperations.size(), 1);

  engine.removeSink(sink);
  QCoreApplication::processEvents();

  const AudioEngine::StateSnapshot state = engine.stateSnapshot();
  QVERIFY(state.pendingOperations.isEmpty());
  QVERIFY(state.logicalEndpoints.isEmpty());
}

void AudioEngineSnapshotTest::panelUnloadQueuedVolumeCommitSafeDuringEngineDestruction() {
  auto* engine = new SnapshotDummyEngine;
  engine->setCommitBehavior(false, true);
  AudioDevice* sink =
      engine->addSink(QStringLiteral("alsa_output.panel-unload"), QStringLiteral("Panel Unload Sink"), 8U, 30, false, 1);
  QVERIFY(sink != nullptr);

  sink->setVolume(75);
  delete engine;

  // Panel unload destroys backend objects before queued commit callbacks drain.
  QCoreApplication::processEvents();
  QVERIFY(true);
}

void AudioEngineSnapshotTest::sessionLogoutQueuedMuteCommitSafeDuringEngineDestruction() {
  auto* engine = new SnapshotDummyEngine;
  engine->setCommitBehavior(true, false);
  AudioDevice* sink = engine->addSink(QStringLiteral("alsa_output.session-logout"),
                                      QStringLiteral("Session Logout Sink"), 9U, 30, false, 1);
  QVERIFY(sink != nullptr);

  sink->setMute(true);
  delete engine;

  // Session logout tears down plugin/backend while queued mute callbacks may still exist.
  QCoreApplication::processEvents();
  QVERIFY(true);
}

void AudioEngineSnapshotTest::backendCrashQueuedStateFlushSafeDuringEngineDestruction() {
  auto* engine = new SnapshotDummyEngine;
  QSignalSpy stateChangedSpy(engine, &AudioEngine::stateChanged);

  engine->setBackendHealthForTest(AudioEngine::BackendHealthState::Reconnecting, QStringLiteral("backend crash"));
  delete engine;

  QTest::qWait(50);
  QCOMPARE(stateChangedSpy.count(), 0);
}

QTEST_MAIN(AudioEngineSnapshotTest)

#include "audioengine_snapshot_test.moc"
