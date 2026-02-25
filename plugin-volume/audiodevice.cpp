/* plugin-volume/audiodevice.cpp
 * Volume control plugin implementation
 */

#include "audiodevice.h"
#include "audioengine.h"

#include <QMetaObject>
#include <QPointer>

AudioDevice::AudioDevice(AudioDeviceType t, AudioEngine* engine, QObject* parent)
    : QObject(parent),
      m_engine(engine),
      m_volume(0),
      m_mute(false),
      m_enabled(true),
      m_type(t),
      m_index(0),
      m_profileName(),
      m_cardId(-1) {}

AudioDevice::~AudioDevice() {
  if (m_engine) {
    m_engine->clearTrackingForDevice(this);
  }
}

void AudioDevice::setName(const QString& name) {
  if (m_name == name)
    return;

  m_name = name;
  emit nameChanged(m_name);
}

void AudioDevice::setDescription(const QString& description) {
  if (m_description == description)
    return;

  m_description = description;
  emit descriptionChanged(m_description);
}

void AudioDevice::setProfileName(const QString& profile) {
  if (m_profileName == profile)
    return;

  m_profileName = profile;
  emit profileNameChanged(m_profileName);
}

void AudioDevice::setIndex(uint index) {
  if (m_index == index)
    return;

  m_index = index;
  emit indexChanged(index);
}

void AudioDevice::setType(AudioDeviceType type) {
  if (m_type == type)
    return;

  m_type = type;
  emit typeChanged(m_type);
}

// this is just for setting the internal volume
void AudioDevice::setVolumeNoCommit(int volume) {
  if (m_engine)
    volume = m_engine->volumeBounded(volume, this);

  const bool changed = (m_volume != volume);
  if (changed) {
    m_volume = volume;
    emit volumeChanged(m_volume);
  }

  if (m_engine) {
    m_engine->completePendingVolumeOperation(this, volume);
    m_engine->markChangeSource(this, AudioEngine::ChangeSource::BackendEvent);
  }
}

void AudioDevice::toggleMute() {
  setMute(!m_mute);
}

void AudioDevice::setMute(bool state) {
  if (m_mute == state)
    return;

  const bool previousState = m_mute;
  m_mute = state;
  emit muteChanged(m_mute);

  if (m_engine) {
    m_engine->beginPendingMuteOperation(this, previousState, state);
    m_engine->markChangeSource(this, AudioEngine::ChangeSource::UserAction);

    QPointer<AudioEngine> engine(m_engine);
    QPointer<AudioDevice> self(this);
    QMetaObject::invokeMethod(
        m_engine,
        [engine, self, state]() {
          if (!engine || !self) {
            return;
          }
          engine->requestSetMute(self, state);
        },
        Qt::QueuedConnection);
  }
}

void AudioDevice::setMuteNoCommit(bool state) {
  const bool changed = (m_mute != state);
  if (changed) {
    m_mute = state;
    emit muteChanged(m_mute);
  }

  if (m_engine) {
    m_engine->completePendingMuteOperation(this, state);
    m_engine->markChangeSource(this, AudioEngine::ChangeSource::BackendEvent);
  }
}

// this performs a volume change on the device
void AudioDevice::setVolume(int volume) {
  if (m_engine)
    volume = m_engine->volumeBounded(volume, this);

  if (m_volume == volume)
    return;

  const int previousVolume = m_volume;
  m_volume = volume;
  emit volumeChanged(m_volume);

  if (m_engine) {
    m_engine->beginPendingVolumeOperation(this, previousVolume, volume);
    m_engine->markChangeSource(this, AudioEngine::ChangeSource::UserAction);

    QPointer<AudioEngine> engine(m_engine);
    QPointer<AudioDevice> self(this);
    QMetaObject::invokeMethod(
        m_engine,
        [engine, self]() {
          if (!engine || !self) {
            return;
          }
          engine->requestCommitDeviceVolume(self);
        },
        Qt::QueuedConnection);
  }
}

void AudioDevice::setEnabled(bool enabled) {
  if (m_enabled == enabled)
    return;

  m_enabled = enabled;
  emit enabledChanged(m_enabled);
}

void AudioDevice::setCardId(int cardId) {
  if (m_cardId == cardId)
    return;

  m_cardId = cardId;
  emit cardIdChanged(m_cardId);
}
