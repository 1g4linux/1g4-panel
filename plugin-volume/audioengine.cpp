/* plugin-volume/audioengine.cpp
 * Volume control plugin implementation
 */

#include "audioengine.h"

#include "audiodevice.h"

#include <QHash>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

QString derivePhysicalKeyFromName(const QString& deviceName) {
  static const QStringList kPrefixes{
      QStringLiteral("alsa_output."),
      QStringLiteral("alsa_input."),
      QStringLiteral("bluez_output."),
      QStringLiteral("bluez_input."),
  };

  for (const QString& prefix : kPrefixes) {
    if (!deviceName.startsWith(prefix)) {
      continue;
    }

    QString base = deviceName.mid(prefix.size());
    const int lastDot = base.lastIndexOf(QLatin1Char('.'));
    if (lastDot > 0) {
      base = base.left(lastDot);
    }
    return base.trimmed();
  }

  return {};
}

QString makePhysicalStableId(const AudioDevice& device) {
  if (device.cardId() >= 0) {
    return QStringLiteral("physical:card=%1").arg(device.cardId());
  }

  const QString keyFromName = derivePhysicalKeyFromName(device.name());
  if (!keyFromName.isEmpty()) {
    return QStringLiteral("physical:name=%1").arg(keyFromName);
  }

  if (!device.name().trimmed().isEmpty()) {
    return QStringLiteral("physical:node=%1").arg(device.name().trimmed());
  }

  return QStringLiteral("physical:runtime=%1").arg(device.index());
}

QString makeEndpointStableId(const AudioDevice& device) {
  const QString name = device.name().trimmed();
  const QString cardId = (device.cardId() >= 0) ? QString::number(device.cardId()) : QStringLiteral("none");

  return QStringLiteral("endpoint:node=%1;card=%2").arg(name, cardId);
}

QString pendingOperationKey(const QString& endpointStableId, AudioEngine::PendingOperationKind kind) {
  return QStringLiteral("%1|%2").arg(endpointStableId, QString::number(static_cast<int>(kind)));
}

QString pendingOperationId(AudioEngine::PendingOperationKind kind, quint64 seq) {
  const QString kindName = (kind == AudioEngine::PendingOperationKind::SetVolume) ? QStringLiteral("set-volume")
                                                                                   : QStringLiteral("set-mute");
  return QStringLiteral("%1-%2").arg(kindName, QString::number(seq));
}

}  // namespace

AudioEngine::AudioEngine(QObject* parent)
    : QObject(parent),
      m_ignoreMaxVolume(false),
      m_nextPendingOperationId(0U),
      m_backendHealth{
          BackendHealthState::Unknown,
          0U,
          QString(),
      } {
  m_discoveryStateCoalesceTimer.setSingleShot(true);
  m_discoveryStateCoalesceTimer.setInterval(kStateChangedCoalesceIntervalMs);
  connect(this, &AudioEngine::sinkListChanged, this, &AudioEngine::queueStateChangedFromDiscovery);
  connect(&m_discoveryStateCoalesceTimer, &QTimer::timeout, this, &AudioEngine::flushDeferredStateChanged);
}

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

AudioEngine::BackendCapabilities AudioEngine::backendCapabilities() const {
  return BackendCapabilities{
      false,
      false,
      false,
      false,
  };
}

QVariantMap AudioEngine::persistenceHints() const {
  return {};
}

bool AudioEngine::setDefaultOutputDevice(const QString& endpointStableId) {
  Q_UNUSED(endpointStableId);
  return false;
}

bool AudioEngine::setDefaultInputDevice(const QString& endpointStableId) {
  Q_UNUSED(endpointStableId);
  return false;
}

bool AudioEngine::movePlaybackStreamToOutput(const QString& streamStableId, const QString& outputEndpointStableId) {
  Q_UNUSED(streamStableId);
  Q_UNUSED(outputEndpointStableId);
  return false;
}

bool AudioEngine::moveRecordingStreamToInput(const QString& streamStableId, const QString& inputEndpointStableId) {
  Q_UNUSED(streamStableId);
  Q_UNUSED(inputEndpointStableId);
  return false;
}

bool AudioEngine::setPhysicalDeviceProfile(const QString& physicalDeviceStableId, const QString& profileName) {
  Q_UNUSED(physicalDeviceStableId);
  Q_UNUSED(profileName);
  return false;
}

AudioEngine::StateSnapshot AudioEngine::stateSnapshot() const {
  StateSnapshot snapshot;
  snapshot.capabilities = backendCapabilities();
  snapshot.backendHealth = m_backendHealth;
  snapshot.logicalEndpoints.reserve(m_sinks.size());

  QHash<QString, int> physicalIndexByStableId;
  physicalIndexByStableId.reserve(m_sinks.size());

  for (const AudioDevice* device : std::as_const(m_sinks)) {
    Q_ASSERT(device != nullptr);

    const QString physicalStableId = makePhysicalStableId(*device);

    auto physicalIt = physicalIndexByStableId.constFind(physicalStableId);
    if (physicalIt == physicalIndexByStableId.cend()) {
      PhysicalDeviceSnapshot physical;
      physical.stableId = physicalStableId;
      physical.displayName = device->description();
      if (physical.displayName.trimmed().isEmpty()) {
        physical.displayName = device->name();
      }
      physical.cardId = device->cardId();
      physical.enabled = device->enabled();
      snapshot.physicalDevices.append(std::move(physical));
      const int newIndex = snapshot.physicalDevices.size() - 1;
      physicalIndexByStableId.insert(physicalStableId, newIndex);
      physicalIt = physicalIndexByStableId.constFind(physicalStableId);
    }
    else {
      PhysicalDeviceSnapshot& physical = snapshot.physicalDevices[*physicalIt];
      physical.enabled = physical.enabled || device->enabled();
      if (physical.displayName.trimmed().isEmpty()) {
        physical.displayName = device->description().trimmed().isEmpty() ? device->name() : device->description();
      }
    }

    LogicalEndpointSnapshot endpoint;
    endpoint.stableId = makeEndpointStableId(*device);
    endpoint.physicalDeviceStableId = physicalStableId;
    endpoint.runtimeId = device->index();
    endpoint.direction = (device->type() == Source) ? EndpointDirection::Input : EndpointDirection::Output;
    endpoint.name = device->name();
    endpoint.description = device->description();
    endpoint.profileName = device->profileName();
    endpoint.cardId = device->cardId();
    endpoint.volumePercent = std::clamp(device->volume(), 0, 100);
    endpoint.muted = device->mute();
    endpoint.enabled = device->enabled();
    endpoint.lastChangeSource = m_lastChangeSourceByEndpoint.value(endpoint.stableId, ChangeSource::Unknown);
    snapshot.logicalEndpoints.append(std::move(endpoint));
  }

  snapshot.pendingOperations.reserve(m_pendingOperationsByKey.size());
  for (const PendingOperationSnapshot& pending : std::as_const(m_pendingOperationsByKey)) {
    snapshot.pendingOperations.append(pending);
  }
  std::sort(snapshot.pendingOperations.begin(), snapshot.pendingOperations.end(),
            [](const PendingOperationSnapshot& left, const PendingOperationSnapshot& right) {
              return left.operationId < right.operationId;
            });

  return snapshot;
}

QList<AudioEngine::SinkSnapshot> AudioEngine::sinkSnapshots() const {
  QList<SinkSnapshot> sinks;
  const StateSnapshot state = stateSnapshot();
  sinks.reserve(state.logicalEndpoints.size());

  for (const LogicalEndpointSnapshot& endpoint : state.logicalEndpoints) {
    if (endpoint.direction != EndpointDirection::Output) {
      continue;
    }

    SinkSnapshot sink;
    sink.stableId = endpoint.stableId;
    sink.runtimeId = endpoint.runtimeId;
    sink.name = endpoint.name;
    sink.description = endpoint.description;
    sink.profileName = endpoint.profileName;
    sink.cardId = endpoint.cardId;
    sink.volumePercent = endpoint.volumePercent;
    sink.muted = endpoint.muted;
    sink.enabled = endpoint.enabled;
    sinks.append(std::move(sink));
  }

  return sinks;
}

void AudioEngine::requestCommitDeviceVolume(AudioDevice* device) {
  if (!commitDeviceVolume(device)) {
    rollbackPendingVolumeOperation(device);
  }
}

void AudioEngine::requestSetMute(AudioDevice* device, bool state) {
  if (!setMute(device, state)) {
    rollbackPendingMuteOperation(device);
  }
}

void AudioEngine::mute(AudioDevice* device) {
  requestSetMute(device, true);
}

void AudioEngine::unmute(AudioDevice* device) {
  requestSetMute(device, false);
}

void AudioEngine::setIgnoreMaxVolume(bool ignore) {
  m_ignoreMaxVolume = ignore;
}

void AudioEngine::beginPendingVolumeOperation(AudioDevice* device, int previousVolumePercent, int targetVolumePercent) {
  if (!device) {
    return;
  }

  const QString endpointStableId = makeEndpointStableId(*device);
  PendingOperationSnapshot pending;
  pending.operationId = pendingOperationId(PendingOperationKind::SetVolume, ++m_nextPendingOperationId);
  pending.endpointStableId = endpointStableId;
  pending.kind = PendingOperationKind::SetVolume;
  pending.previousVolumePercent = std::clamp(previousVolumePercent, 0, 100);
  pending.targetVolumePercent = std::clamp(targetVolumePercent, 0, 100);
  pending.previousMuted = device->mute();
  pending.targetMuted = device->mute();

  m_pendingOperationsByKey.insert(pendingOperationKey(endpointStableId, PendingOperationKind::SetVolume), pending);
  m_lastChangeSourceByEndpoint.insert(endpointStableId, ChangeSource::UserAction);
  queueStateChangedByObjectAndType(endpointStableId, CoalescedStateEventType::EndpointState);
}

void AudioEngine::beginPendingMuteOperation(AudioDevice* device, bool previousMuted, bool targetMuted) {
  if (!device) {
    return;
  }

  const QString endpointStableId = makeEndpointStableId(*device);
  PendingOperationSnapshot pending;
  pending.operationId = pendingOperationId(PendingOperationKind::SetMute, ++m_nextPendingOperationId);
  pending.endpointStableId = endpointStableId;
  pending.kind = PendingOperationKind::SetMute;
  pending.previousVolumePercent = device->volume();
  pending.targetVolumePercent = device->volume();
  pending.previousMuted = previousMuted;
  pending.targetMuted = targetMuted;

  m_pendingOperationsByKey.insert(pendingOperationKey(endpointStableId, PendingOperationKind::SetMute), pending);
  m_lastChangeSourceByEndpoint.insert(endpointStableId, ChangeSource::UserAction);
  queueStateChangedByObjectAndType(endpointStableId, CoalescedStateEventType::EndpointState);
}

void AudioEngine::completePendingVolumeOperation(AudioDevice* device, int appliedVolumePercent) {
  Q_UNUSED(appliedVolumePercent);
  if (!device) {
    return;
  }

  const QString endpointStableId = makeEndpointStableId(*device);
  const int removed = m_pendingOperationsByKey.remove(pendingOperationKey(endpointStableId, PendingOperationKind::SetVolume));
  m_lastChangeSourceByEndpoint.insert(endpointStableId, ChangeSource::BackendEvent);
  if (removed > 0) {
    queueStateChangedByObjectAndType(endpointStableId, CoalescedStateEventType::EndpointState);
  }
}

void AudioEngine::completePendingMuteOperation(AudioDevice* device, bool appliedMuted) {
  Q_UNUSED(appliedMuted);
  if (!device) {
    return;
  }

  const QString endpointStableId = makeEndpointStableId(*device);
  const int removed = m_pendingOperationsByKey.remove(pendingOperationKey(endpointStableId, PendingOperationKind::SetMute));
  m_lastChangeSourceByEndpoint.insert(endpointStableId, ChangeSource::BackendEvent);
  if (removed > 0) {
    queueStateChangedByObjectAndType(endpointStableId, CoalescedStateEventType::EndpointState);
  }
}

void AudioEngine::rollbackPendingVolumeOperation(AudioDevice* device) {
  if (!device) {
    return;
  }

  const QString endpointStableId = makeEndpointStableId(*device);
  const QString key = pendingOperationKey(endpointStableId, PendingOperationKind::SetVolume);
  const auto it = m_pendingOperationsByKey.constFind(key);
  if (it == m_pendingOperationsByKey.cend()) {
    return;
  }

  const PendingOperationSnapshot pending = *it;
  m_pendingOperationsByKey.remove(key);
  device->setVolumeNoCommit(pending.previousVolumePercent);
  m_lastChangeSourceByEndpoint.insert(endpointStableId, ChangeSource::Rollback);
  queueStateChangedByObjectAndType(endpointStableId, CoalescedStateEventType::EndpointState);
}

void AudioEngine::rollbackPendingMuteOperation(AudioDevice* device) {
  if (!device) {
    return;
  }

  const QString endpointStableId = makeEndpointStableId(*device);
  const QString key = pendingOperationKey(endpointStableId, PendingOperationKind::SetMute);
  const auto it = m_pendingOperationsByKey.constFind(key);
  if (it == m_pendingOperationsByKey.cend()) {
    return;
  }

  const PendingOperationSnapshot pending = *it;
  m_pendingOperationsByKey.remove(key);
  device->setMuteNoCommit(pending.previousMuted);
  m_lastChangeSourceByEndpoint.insert(endpointStableId, ChangeSource::Rollback);
  queueStateChangedByObjectAndType(endpointStableId, CoalescedStateEventType::EndpointState);
}

void AudioEngine::markChangeSource(AudioDevice* device, ChangeSource source) {
  if (!device) {
    return;
  }

  const QString endpointStableId = makeEndpointStableId(*device);
  const auto currentIt = m_lastChangeSourceByEndpoint.constFind(endpointStableId);
  if (currentIt != m_lastChangeSourceByEndpoint.cend() && *currentIt == source) {
    return;
  }

  m_lastChangeSourceByEndpoint.insert(endpointStableId, source);
  queueStateChangedByObjectAndType(endpointStableId, CoalescedStateEventType::EndpointState);
}

void AudioEngine::clearTrackingForDevice(const AudioDevice* device) {
  if (!device) {
    return;
  }

  const QString endpointStableId = makeEndpointStableId(*device);
  bool changed = false;
  changed = m_lastChangeSourceByEndpoint.remove(endpointStableId) > 0 || changed;
  changed = m_pendingOperationsByKey.remove(pendingOperationKey(endpointStableId, PendingOperationKind::SetVolume)) > 0 ||
            changed;
  changed = m_pendingOperationsByKey.remove(pendingOperationKey(endpointStableId, PendingOperationKind::SetMute)) > 0 ||
            changed;

  if (changed) {
    queueStateChangedByObjectAndType(endpointStableId, CoalescedStateEventType::EndpointState);
  }
}

void AudioEngine::setBackendHealth(BackendHealthState state, const QString& message) {
  if (m_backendHealth.state == state && m_backendHealth.message == message) {
    return;
  }

  m_backendHealth.state = state;
  m_backendHealth.message = message;
  queueStateChangedByObjectAndType(QStringLiteral("backend"), CoalescedStateEventType::BackendHealth);
}

void AudioEngine::recordReconnectAttempt(const QString& message) {
  ++m_backendHealth.reconnectAttempts;
  m_backendHealth.state = BackendHealthState::Reconnecting;
  m_backendHealth.message = message;
  queueStateChangedByObjectAndType(QStringLiteral("backend"), CoalescedStateEventType::BackendHealth);
}

void AudioEngine::queueStateChangedFromDiscovery() {
  queueStateChangedByObjectAndType(QStringLiteral("discovery:sinks"), CoalescedStateEventType::Discovery);
}

void AudioEngine::queueStateChangedByObjectAndType(const QString& objectKey, CoalescedStateEventType eventType) {
  QString stableObjectKey = objectKey.trimmed();
  if (stableObjectKey.isEmpty()) {
    stableObjectKey = QStringLiteral("global");
  }

  const QString eventKey =
      QStringLiteral("%1|%2").arg(stableObjectKey, QString::number(static_cast<int>(eventType)));
  m_pendingCoalescedStateEvents.insert(eventKey);

  if (!m_discoveryStateCoalesceTimer.isActive()) {
    m_discoveryStateCoalesceTimer.start();
  }
}

void AudioEngine::flushDeferredStateChanged() {
  if (m_pendingCoalescedStateEvents.isEmpty()) {
    return;
  }

  m_pendingCoalescedStateEvents.clear();
  emit stateChanged();
}
