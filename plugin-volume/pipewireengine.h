/* plugin-volume/pipewireengine.h
 * Volume control plugin implementation
 */

#ifndef PIPEWIREENGINE_H
#define PIPEWIREENGINE_H

#include "audioengine.h"
#include "audiodevice.h"

#include <QObject>
#include <QList>
#include <QSet>
#include <QTimer>
#include <QMap>

#include <atomic>

#include <pipewire/pipewire.h>
#include <pipewire/thread-loop.h>
#include <pipewire/extensions/metadata.h>
#include <spa/utils/hook.h>
#include <spa/utils/dict.h>
class AudioDevice;

class PipeWireEngine : public AudioEngine {
  Q_OBJECT

 public:
  explicit PipeWireEngine(QObject* parent = nullptr);
  ~PipeWireEngine() override;

  virtual const QString backendName() const override { return QLatin1String("PipeWire"); }
  int volumeMax(AudioDevice* device) const override;
  bool setDeviceEnabled(AudioDevice* device, bool enabled) override;
  bool deviceIsEnabled(AudioDevice* device) const override;

 public slots:
  bool commitDeviceVolume(AudioDevice* device) override;
  bool setMute(AudioDevice* device, bool state) override;
  void setIgnoreMaxVolume(bool ignore) override;

 signals:
  void contextStateChanged(bool ready);
  void sinkInfoChanged(uint32_t nodeId);
  void readyChanged(bool ready);

 private slots:
  void handleContextStateChanged();
  void reconnect();

 private:
  struct NodeInfo {
    uint32_t id;
    QString name;
    QString description;
    bool isSink;
  };

  // pipewire objects
  pw_thread_loop* m_threadLoop;
  pw_context* m_context;
  pw_core* m_core;
  pw_registry* m_registry;
  pw_metadata* m_metadata;

  spa_hook m_coreListener;
  spa_hook m_registryListener;
  spa_hook m_metadataListener;

  bool m_ready;
  bool m_connecting;
  std::atomic_bool m_isShuttingDown;
  int m_maximumVolume;

  QTimer m_reconnectionTimer;

  QMap<AudioDevice*, uint32_t> m_nodeIdByDevice;
  QMap<uint32_t, AudioDevice*> m_deviceByWpId;
  QMap<uint32_t, pw_node*> m_nodeByNodeId;
  QMap<uint32_t, spa_hook> m_nodeListenerByNodeId;
  QMap<uint32_t, void*> m_nodeListenerDataByNodeId;
  QSet<uint32_t> m_disabledNodeIds;
  uint32_t m_metadataId;
  QString m_defaultOutputNodeName;
  QString m_defaultInputNodeName;

  void connectContext();
  void disconnectContext();

  void addOrUpdateNode(uint32_t id,
                       const QString& name,
                       const QString& description,
                       AudioDeviceType type,
                       int cardId,
                       const QString& profileName);
  void removeNode(uint32_t id);
  void bindNode(uint32_t id);
  void unbindNode(uint32_t id);
  void bindMetadata(uint32_t id);
  void unbindMetadata(uint32_t id);
  void applyNodeParamUpdate(uint32_t nodeId, bool hasVolume, float volume, bool hasMute, bool mute);
  void applyNodeDisabledUpdate(uint32_t nodeId, bool disabled);
  void applyDefaultNodeNameUpdate(AudioEngine::EndpointDirection direction, const QString& nodeName);

  void queryNodeVolume(uint32_t nodeId);
  bool setNodeVolume(uint32_t nodeId, float volume);
  bool setNodeMute(uint32_t nodeId, bool mute);
  bool setNodeDisabledMetadata(uint32_t nodeId, bool disabled);
  void setNodeEnabledState(AudioDevice* dev, bool enabled);
  bool isShuttingDown() const noexcept { return m_isShuttingDown.load(std::memory_order_acquire); }

  void setReady(bool ready);

  // static callbacks for pipewire
  static void onRegistryGlobal(void* data,
                               uint32_t id,
                               uint32_t permissions,
                               const char* type,
                               uint32_t version,
                               const spa_dict* props);

  static void onRegistryGlobalRemove(void* data, uint32_t id);
  static int onMetadataProperty(void* data, uint32_t subject, const char* key, const char* type, const char* value);

  static void onCoreError(void* data, uint32_t id, int seq, int res, const char* message);

  static void onNodeInfo(void* data, const pw_node_info* info);
  static void onNodeParams(void* data, int seq, uint32_t id, uint32_t index, uint32_t next, const spa_pod* param);
};

#endif  // PIPEWIREENGINE_H
