#include "audiodevice.h"
#include "audioengine.h"

#include <QtTest/QtTest>

class DummyAudioEngine : public AudioEngine {
  Q_OBJECT

 public:
  explicit DummyAudioEngine(int maximumVolume, QObject* parent = nullptr)
      : AudioEngine(parent), m_maximumVolume(maximumVolume) {}

  int volumeMax(AudioDevice* /*device*/) const override {
    return m_maximumVolume;
  }

  const QString backendName() const override {
    return QStringLiteral("dummy");
  }

  void commitDeviceVolume(AudioDevice* /*device*/) override {}
  void setMute(AudioDevice* /*device*/, bool /*state*/) override {}

  bool ignoreMaxVolumeFlag() const {
    return m_ignoreMaxVolume;
  }

 private:
  int m_maximumVolume;
};

class AudioEngineIgnoreMaxVolumeTest : public QObject {
  Q_OBJECT

 private slots:
  void ignoreMaxVolumeIsEngineInstanceScoped();
};

void AudioEngineIgnoreMaxVolumeTest::ignoreMaxVolumeIsEngineInstanceScoped() {
  DummyAudioEngine firstEngine(100);
  DummyAudioEngine secondEngine(100);

  AudioDevice firstDevice(Sink, &firstEngine);
  AudioDevice secondDevice(Sink, &secondEngine);

  firstEngine.setIgnoreMaxVolume(true);
  secondEngine.setIgnoreMaxVolume(false);

  QVERIFY(firstEngine.ignoreMaxVolumeFlag());
  QVERIFY(!secondEngine.ignoreMaxVolumeFlag());

  QCOMPARE(firstEngine.volumeBounded(73, &firstDevice), 73);
  QCOMPARE(secondEngine.volumeBounded(73, &secondDevice), 73);
}

QTEST_MAIN(AudioEngineIgnoreMaxVolumeTest)

#include "audioengine_ignoremaxvolume_test.moc"
