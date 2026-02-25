#include "audiodevice.h"
#include "audioengine.h"

#include <QtTest/QtTest>

class SnapshotDummyEngine : public AudioEngine {
  Q_OBJECT

 public:
  explicit SnapshotDummyEngine(QObject* parent = nullptr) : AudioEngine(parent) {}

  int volumeMax(AudioDevice* /*device*/) const override {
    return 100;
  }

  const QString backendName() const override {
    return QStringLiteral("snapshot-dummy");
  }

  void commitDeviceVolume(AudioDevice* /*device*/) override {}
  void setMute(AudioDevice* /*device*/, bool /*state*/) override {}

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
};

class AudioEngineSnapshotTest : public QObject {
  Q_OBJECT

 private slots:
  void snapshotIsDetachedFromMutableDeviceState();
  void stableIdDoesNotDependOnRuntimeId();
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

QTEST_MAIN(AudioEngineSnapshotTest)

#include "audioengine_snapshot_test.moc"
