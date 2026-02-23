/* plugin-volume/audioengine.cpp
 * Volume control plugin implementation
 */

#include "audioengine.h"

#include "audiodevice.h"

#include <algorithm>
#include <cmath>
#include <utility>

AudioEngine::AudioEngine(QObject* parent) : QObject(parent), m_ignoreMaxVolume(false) {}

AudioEngine::~AudioEngine() {
  for (AudioDevice* device : std::as_const(m_sinks)) {
    delete device;
  }
  m_sinks.clear();
}

int AudioEngine::volumeBounded(int volume, AudioDevice* device) const {
  volume = std::clamp(volume, 0, 100);

  if (!device) {
    return volume;
  }

  const int maximum = volumeMax(device);
  if (maximum <= 0) {
    return 0;
  }

  if (m_ignoreMaxVolume) {
    return volume;
  }

  const double v = static_cast<double>(volume) / 100.0 * static_cast<double>(maximum);
  const double bounded = std::clamp(v, 0.0, static_cast<double>(maximum));
  return static_cast<int>(std::lround(bounded / static_cast<double>(maximum) * 100.0));
}

void AudioEngine::mute(AudioDevice* device) {
  setMute(device, true);
}

void AudioEngine::unmute(AudioDevice* device) {
  setMute(device, false);
}

void AudioEngine::setIgnoreMaxVolume(bool ignore) {
  m_ignoreMaxVolume = ignore;
}
