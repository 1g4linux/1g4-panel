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

bool TestAudioEngine::commitDeviceVolume(AudioDevice* device) {
  if (!device) {
    return false;
  }

  device->setVolumeNoCommit(volumeBounded(device->volume(), device));
  return true;
}

bool TestAudioEngine::setMute(AudioDevice* device, bool state) {
  if (!device) {
    return false;
  }

  device->setMuteNoCommit(state);
  return true;
}
