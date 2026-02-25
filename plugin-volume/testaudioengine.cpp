/* plugin-volume/testaudioengine.cpp
 * Development-only in-process backend used for UI and plugin tests.
 */

#include "testaudioengine.h"

#include "audiodevice.h"

TestAudioEngine::TestAudioEngine(QObject* parent) : AudioEngine(parent) {
  auto* sink = new AudioDevice(Sink, this, this);
  sink->setIndex(1U);
  sink->setName(QStringLiteral("test_output.default"));
  sink->setDescription(QStringLiteral("Test Output"));
  sink->setVolumeNoCommit(50);
  sink->setMuteNoCommit(false);
  m_sinks.append(sink);
}

int TestAudioEngine::volumeMax(AudioDevice* device) const {
  return device ? 100 : 0;
}

void TestAudioEngine::commitDeviceVolume(AudioDevice* device) {
  if (!device) {
    return;
  }

  device->setVolumeNoCommit(volumeBounded(device->volume(), device));
}

void TestAudioEngine::setMute(AudioDevice* device, bool state) {
  if (!device) {
    return;
  }

  device->setMuteNoCommit(state);
}
