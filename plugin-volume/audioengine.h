/* plugin-volume/audioengine.h
 * Volume control plugin implementation
 */

#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#include <QHash>
#include <QObject>
#include <QList>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QtGlobal>

class AudioDevice;

class AudioEngine : public QObject {
  Q_OBJECT
  friend class AudioDevice;

 public:
  enum class PendingOperationKind { SetVolume, SetMute };
  enum class ChangeSource { Unknown, UserAction, BackendEvent, Rollback };
  enum class BackendHealthState { Unknown, Ready, Reconnecting, Unavailable };

  struct BackendHealthSnapshot {
    BackendHealthState state;
    quint32 reconnectAttempts;
    QString message;
  };

  struct BackendCapabilities {
    bool profileSwitchingSupported;
    bool streamMoveSupported;
    bool batteryAvailable;
    bool perPortSelectionSupported;
  };

  struct PhysicalDeviceSnapshot {
    QString stableId;
    QString displayName;
    int cardId;
    bool enabled;
  };

  enum class EndpointDirection { Output, Input };

  struct LogicalEndpointSnapshot {
    QString stableId;
    QString physicalDeviceStableId;
    uint runtimeId;
    EndpointDirection direction;
    QString name;
    QString description;
    QString profileName;
    int cardId;
    int volumePercent;
    bool muted;
    bool enabled;
    ChangeSource lastChangeSource;
  };

  enum class StreamDirection { Playback, Recording };

  struct StreamSnapshot {
    QString stableId;
    StreamDirection direction;
    QString name;
    int volumePercent;
    bool muted;
  };

  struct PendingOperationSnapshot {
    QString operationId;
    QString endpointStableId;
    PendingOperationKind kind;
    int previousVolumePercent;
    int targetVolumePercent;
    bool previousMuted;
    bool targetMuted;
  };

  struct StateSnapshot {
    BackendCapabilities capabilities;
    BackendHealthSnapshot backendHealth;
    QList<PhysicalDeviceSnapshot> physicalDevices;
    QList<LogicalEndpointSnapshot> logicalEndpoints;
    QList<StreamSnapshot> streams;
    QList<PendingOperationSnapshot> pendingOperations;
  };

  struct SinkSnapshot {
    QString stableId;
    uint runtimeId;
    QString name;
    QString description;
    QString profileName;
    int cardId;
    int volumePercent;
    bool muted;
    bool enabled;
  };

  AudioEngine(QObject* parent = nullptr);
  ~AudioEngine();

  const QList<AudioDevice*>& sinks() const { return m_sinks; }
  StateSnapshot stateSnapshot() const;
  QList<SinkSnapshot> sinkSnapshots() const;
  virtual BackendCapabilities backendCapabilities() const;
  BackendHealthSnapshot backendHealth() const { return m_backendHealth; }
  virtual QVariantMap persistenceHints() const;
  virtual bool setDefaultOutputDevice(const QString& endpointStableId);
  virtual bool setDefaultInputDevice(const QString& endpointStableId);
  virtual bool movePlaybackStreamToOutput(const QString& streamStableId, const QString& outputEndpointStableId);
  virtual bool moveRecordingStreamToInput(const QString& streamStableId, const QString& inputEndpointStableId);
  virtual bool setPhysicalDeviceProfile(const QString& physicalDeviceStableId, const QString& profileName);
  virtual int volumeMax(AudioDevice* device) const = 0;
  virtual int volumeBounded(int volume, AudioDevice* device) const;
  virtual const QString backendName() const = 0;
  virtual bool setDeviceEnabled(AudioDevice* /*device*/, bool /*enabled*/) { return false; }
  virtual bool deviceIsEnabled(AudioDevice* /*device*/) const { return true; }

 public slots:
  void requestCommitDeviceVolume(AudioDevice* device);
  void requestSetMute(AudioDevice* device, bool state);
  void mute(AudioDevice* device);
  void unmute(AudioDevice* device);
  virtual void setIgnoreMaxVolume(bool ignore);

 signals:
  void sinkListChanged();
  void stateChanged();

 protected:
  virtual bool commitDeviceVolume(AudioDevice* device) = 0;
  virtual bool setMute(AudioDevice* device, bool state) = 0;

  void beginPendingVolumeOperation(AudioDevice* device, int previousVolumePercent, int targetVolumePercent);
  void beginPendingMuteOperation(AudioDevice* device, bool previousMuted, bool targetMuted);
  void completePendingVolumeOperation(AudioDevice* device, int appliedVolumePercent);
  void completePendingMuteOperation(AudioDevice* device, bool appliedMuted);
  void rollbackPendingVolumeOperation(AudioDevice* device);
  void rollbackPendingMuteOperation(AudioDevice* device);
  void markChangeSource(AudioDevice* device, ChangeSource source);
  void clearTrackingForDevice(const AudioDevice* device);
  void setBackendHealth(BackendHealthState state, const QString& message = {});
  void recordReconnectAttempt(const QString& message = {});

  QList<AudioDevice*> m_sinks;
  bool m_ignoreMaxVolume;

 private:
  static constexpr int kDiscoveryStateCoalesceIntervalMs = 20;
  void queueStateChangedFromDiscovery();
  void flushDeferredStateChanged();

  quint64 m_nextPendingOperationId;
  QHash<QString, PendingOperationSnapshot> m_pendingOperationsByKey;
  QHash<QString, ChangeSource> m_lastChangeSourceByEndpoint;
  BackendHealthSnapshot m_backendHealth;
  QTimer m_discoveryStateCoalesceTimer;
  bool m_hasPendingDiscoveryState;
};

#endif  // AUDIOENGINE_H
