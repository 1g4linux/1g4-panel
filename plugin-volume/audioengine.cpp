/* plugin-volume/audioengine.cpp
 * Volume control plugin implementation
 */

#include "audioengine.h"

#include "audiodevice.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

QString makeStableSinkId(const AudioDevice& device) {
  const QString name = device.name().trimmed();
  const QString description = device.description().trimmed();
  const QString cardId = (device.cardId() >= 0) ? QString::number(device.cardId()) : QStringLiteral("none");

  return QStringLiteral("sink:name=%1;card=%2;desc=%3").arg(name, cardId, description);
}

}  // namespace

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

QList<AudioEngine::SinkSnapshot> AudioEngine::sinkSnapshots() const {
  QList<SinkSnapshot> snapshots;
  snapshots.reserve(m_sinks.size());

  for (const AudioDevice* device : std::as_const(m_sinks)) {
    Q_ASSERT(device != nullptr);

    SinkSnapshot snapshot;
    snapshot.stableId = makeStableSinkId(*device);
    snapshot.runtimeId = device->index();
    snapshot.name = device->name();
    snapshot.description = device->description();
    snapshot.profileName = device->profileName();
    snapshot.cardId = device->cardId();
    snapshot.volumePercent = std::clamp(device->volume(), 0, 100);
    snapshot.muted = device->mute();
    snapshot.enabled = device->enabled();
    snapshots.append(std::move(snapshot));
  }

  return snapshots;
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
