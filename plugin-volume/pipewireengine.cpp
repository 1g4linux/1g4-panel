/* plugin-volume/pipewireengine.cpp
 * Volume control plugin implementation
 */

#include "pipewireengine.h"
#include "audiodevice.h"
#include "volumelogging.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QLoggingCategory>
#include <QPointer>
#include <QtDebug>
#include <QtGlobal>

#include <spa/param/audio/raw.h>
#include <spa/param/props.h>
#include <spa/pod/builder.h>
#include <spa/pod/iter.h>
#include <spa/pod/pod.h>
#include <spa/utils/defs.h>

#include <algorithm>

namespace {
Q_LOGGING_CATEGORY(lcPipeWireEngine, "oneg4.panel.plugin.volume.pipewire", QtWarningMsg)

struct NodeListenerData {
  PipeWireEngine* engine;
  uint32_t nodeId;
};

QString parseDefaultNodeName(const char* value) {
  if (!value) {
    return {};
  }

  const QString rawValue = QString::fromUtf8(value).trimmed();
  if (rawValue.isEmpty() || rawValue == QLatin1String("null")) {
    return {};
  }

  QJsonParseError parseError;
  const QJsonDocument doc = QJsonDocument::fromJson(rawValue.toUtf8(), &parseError);
  if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
    return doc.object().value(QStringLiteral("name")).toString().trimmed();
  }

  if (rawValue.size() >= 2 && rawValue.startsWith(QLatin1Char('"')) && rawValue.endsWith(QLatin1Char('"'))) {
    return rawValue.mid(1, rawValue.size() - 2).trimmed();
  }

  return rawValue;
}
}  // namespace

PipeWireEngine::PipeWireEngine(QObject* parent)
    : AudioEngine(parent),
      m_threadLoop(nullptr),
      m_context(nullptr),
      m_core(nullptr),
      m_registry(nullptr),
      m_metadata(nullptr),
      m_ready(false),
      m_connecting(false),
      m_isShuttingDown(false),
      m_maximumVolume(150),
      m_metadataId(SPA_ID_INVALID),
      m_defaultOutputNodeName(),
      m_defaultInputNodeName() {
  pw_init(nullptr, nullptr);

  m_reconnectionTimer.setSingleShot(true);
  m_reconnectionTimer.setInterval(200);
  connect(&m_reconnectionTimer, &QTimer::timeout, this, &PipeWireEngine::reconnect);

  m_threadLoop = pw_thread_loop_new("oneg4-volume", nullptr);
  if (!m_threadLoop) {
    qCWarning(lcVolumeBackend) << "PipeWireEngine: unable to create thread loop";
    setBackendHealth(BackendHealthState::Unavailable, QStringLiteral("PipeWire thread loop creation failed"));
    return;
  }

  pw_thread_loop_lock(m_threadLoop);

  m_context = pw_context_new(pw_thread_loop_get_loop(m_threadLoop), nullptr, 0);
  if (!m_context) {
    qCWarning(lcVolumeBackend) << "PipeWireEngine: unable to create context";
    setBackendHealth(BackendHealthState::Unavailable, QStringLiteral("PipeWire context creation failed"));
    pw_thread_loop_unlock(m_threadLoop);
    pw_thread_loop_destroy(m_threadLoop);
    m_threadLoop = nullptr;
    return;
  }

  if (pw_thread_loop_start(m_threadLoop) < 0) {
    qCWarning(lcVolumeBackend) << "PipeWireEngine: unable to start thread loop";
    setBackendHealth(BackendHealthState::Unavailable, QStringLiteral("PipeWire thread loop start failed"));
    pw_context_destroy(m_context);
    m_context = nullptr;
    pw_thread_loop_unlock(m_threadLoop);
    pw_thread_loop_destroy(m_threadLoop);
    m_threadLoop = nullptr;
    return;
  }

  connect(this, &PipeWireEngine::contextStateChanged, this, &PipeWireEngine::handleContextStateChanged);

  connectContext();

  pw_thread_loop_unlock(m_threadLoop);
}

PipeWireEngine::~PipeWireEngine() {
  m_isShuttingDown.store(true, std::memory_order_release);

  for (uint32_t nodeId : m_nodeByNodeId.keys()) {
    unbindNode(nodeId);
  }

  if (m_threadLoop) {
    pw_thread_loop_lock(m_threadLoop);

    if (m_metadata) {
      spa_hook_remove(&m_metadataListener);
      pw_proxy_destroy(reinterpret_cast<pw_proxy*>(m_metadata));
      m_metadata = nullptr;
      m_metadataId = SPA_ID_INVALID;
      spa_zero(m_metadataListener);
    }

    if (m_registry) {
      spa_hook_remove(&m_registryListener);
      m_registry = nullptr;
      spa_zero(m_registryListener);
    }

    if (m_core) {
      spa_hook_remove(&m_coreListener);
      m_core = nullptr;
      spa_zero(m_coreListener);
    }

    if (m_context) {
      pw_context_destroy(m_context);
      m_context = nullptr;
    }

    pw_thread_loop_unlock(m_threadLoop);

    pw_thread_loop_stop(m_threadLoop);
    pw_thread_loop_destroy(m_threadLoop);
    m_threadLoop = nullptr;
  }

  pw_deinit();
}

int PipeWireEngine::volumeMax(AudioDevice* /*device*/) const {
  return m_maximumVolume;
}

bool PipeWireEngine::deviceIsEnabled(AudioDevice* device) const {
  if (!device)
    return false;

  const uint32_t nodeId = m_nodeIdByDevice.value(device, SPA_ID_INVALID);
  if (nodeId != SPA_ID_INVALID && m_disabledNodeIds.contains(nodeId))
    return false;
  return device->enabled();
}

bool PipeWireEngine::setDeviceEnabled(AudioDevice* device, bool enabled) {
  if (!device || !m_ready)
    return false;

  const uint32_t nodeId = m_nodeIdByDevice.value(device, SPA_ID_INVALID);
  if (nodeId == SPA_ID_INVALID)
    return false;

  const bool ok = setNodeDisabledMetadata(nodeId, !enabled);
  if (ok)
    setNodeEnabledState(device, enabled);
  return ok;
}

void PipeWireEngine::connectContext() {
  if (!m_threadLoop || !m_context)
    return;

  if (isShuttingDown())
    return;

  if (m_connecting)
    return;

  m_connecting = true;
  setBackendHealth(BackendHealthState::Reconnecting, QStringLiteral("Connecting to PipeWire"));
  setReady(false);

  pw_thread_loop_lock(m_threadLoop);

  m_core = pw_context_connect(m_context, nullptr, 0);
  if (!m_core) {
    qCWarning(lcVolumeBackend) << "PipeWireEngine: failed to connect to PipeWire core";
    setBackendHealth(BackendHealthState::Reconnecting,
                     QStringLiteral("PipeWire core unavailable, retry scheduled"));
    pw_thread_loop_unlock(m_threadLoop);
    m_connecting = false;
    m_reconnectionTimer.start();
    return;
  }

  static const pw_core_events coreEvents = {
      PW_VERSION_CORE_EVENTS,
      nullptr,
      nullptr,
      nullptr,
      &PipeWireEngine::onCoreError,
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      nullptr,
  };

  spa_zero(m_coreListener);
  pw_core_add_listener(m_core, &m_coreListener, &coreEvents, this);

  m_registry = pw_core_get_registry(m_core, PW_VERSION_REGISTRY, 0);
  if (!m_registry) {
    qCWarning(lcVolumeBackend) << "PipeWireEngine: failed to get registry";
    setBackendHealth(BackendHealthState::Reconnecting, QStringLiteral("PipeWire registry unavailable, retry scheduled"));
    pw_thread_loop_unlock(m_threadLoop);
    m_connecting = false;
    m_reconnectionTimer.start();
    return;
  }

  static const pw_registry_events registryEvents = {
      PW_VERSION_REGISTRY_EVENTS,
      &PipeWireEngine::onRegistryGlobal,
      &PipeWireEngine::onRegistryGlobalRemove,
  };

  spa_zero(m_registryListener);
  pw_registry_add_listener(m_registry, &m_registryListener, &registryEvents, this);

  setReady(true);

  pw_thread_loop_unlock(m_threadLoop);
  m_connecting = false;
}

void PipeWireEngine::disconnectContext() {
  if (!m_threadLoop)
    return;

  // Reconnect must start from a clean snapshot; stale node proxies and runtime ids
  // become invalid once the core disconnects.
  const QList<uint32_t> nodeIds = m_nodeByNodeId.keys();
  for (uint32_t nodeId : nodeIds) {
    unbindNode(nodeId);
  }

  QSet<AudioDevice*> devicesToDelete;
  devicesToDelete.reserve(m_deviceByWpId.size());
  for (AudioDevice* device : std::as_const(m_deviceByWpId)) {
    if (device) {
      devicesToDelete.insert(device);
    }
  }
  m_deviceByWpId.clear();
  m_nodeIdByDevice.clear();
  m_defaultOutputNodeName.clear();
  m_defaultInputNodeName.clear();
  setObservedDefaultEndpointStableId(EndpointDirection::Output, QString());
  setObservedDefaultEndpointStableId(EndpointDirection::Input, QString());

  bool removedEndpoint = false;
  for (AudioDevice* device : std::as_const(devicesToDelete)) {
    if (device->type() == Sink) {
      removedEndpoint = true;
      m_sinks.removeAll(device);
    }
    else if (device->type() == Source) {
      removedEndpoint = true;
      m_sources.removeAll(device);
    }
    delete device;
  }

  pw_thread_loop_lock(m_threadLoop);

  if (m_metadata) {
    spa_hook_remove(&m_metadataListener);
    pw_proxy_destroy(reinterpret_cast<pw_proxy*>(m_metadata));
    m_metadata = nullptr;
  }
  spa_zero(m_metadataListener);
  m_metadataId = SPA_ID_INVALID;
  m_disabledNodeIds.clear();

  if (m_registry) {
    spa_hook_remove(&m_registryListener);
    spa_zero(m_registryListener);
    m_registry = nullptr;
  }

  if (m_core) {
    spa_hook_remove(&m_coreListener);
    spa_zero(m_coreListener);
    pw_core_disconnect(m_core);
    m_core = nullptr;
  }

  setReady(false);

  pw_thread_loop_unlock(m_threadLoop);

  if (removedEndpoint) {
    emit sinkListChanged();
  }
}

void PipeWireEngine::reconnect() {
  recordReconnectAttempt(QStringLiteral("Reconnecting to PipeWire"));
  disconnectContext();
  connectContext();
}

void PipeWireEngine::setReady(bool ready) {
  if (m_ready == ready)
    return;

  m_ready = ready;
  if (m_ready) {
    setBackendHealth(BackendHealthState::Ready, QString());
  }
  else if (m_connecting || m_reconnectionTimer.isActive()) {
    setBackendHealth(BackendHealthState::Reconnecting, QStringLiteral("Waiting for PipeWire backend"));
  }
  else {
    setBackendHealth(BackendHealthState::Unavailable, QStringLiteral("PipeWire backend unavailable"));
  }
  emit contextStateChanged(m_ready);
  emit readyChanged(m_ready);
}

void PipeWireEngine::handleContextStateChanged() {
  if (!m_ready) {
    qCWarning(lcVolumeBackend) << "PipeWireEngine: context not ready, scheduling reconnect";
    setBackendHealth(BackendHealthState::Reconnecting, QStringLiteral("Context not ready, scheduling reconnect"));
    m_reconnectionTimer.start();
  }
}

// static
void PipeWireEngine::onCoreError(void* data, uint32_t id, int seq, int res, const char* message) {
  Q_UNUSED(id);
  Q_UNUSED(seq);
  Q_UNUSED(res);

  auto* engine = static_cast<PipeWireEngine*>(data);
  if (!engine) {
    return;
  }
  if (engine->isShuttingDown()) {
    return;
  }
  const QString errorMessage = QString::fromUtf8(message ? message : "unknown");
  qCWarning(lcVolumeBackend) << "PipeWireEngine: core error" << errorMessage;
  QPointer<PipeWireEngine> enginePtr(engine);
  QMetaObject::invokeMethod(
      engine,
      [enginePtr, errorMessage]() {
        if (!enginePtr || enginePtr->isShuttingDown()) {
          return;
        }
        enginePtr->setBackendHealth(AudioEngine::BackendHealthState::Reconnecting,
                                    QStringLiteral("PipeWire core error: %1").arg(errorMessage));
        enginePtr->setReady(false);
      },
      Qt::QueuedConnection);
}

// static
void PipeWireEngine::onRegistryGlobal(void* data,
                                      uint32_t id,
                                      uint32_t permissions,
                                      const char* type,
                                      uint32_t version,
                                      const spa_dict* props) {
  Q_UNUSED(permissions);
  Q_UNUSED(version);

  auto* engine = static_cast<PipeWireEngine*>(data);
  if (!engine)
    return;
  if (engine->isShuttingDown())
    return;

  QPointer<PipeWireEngine> enginePtr(engine);

  if (strcmp(type, PW_TYPE_INTERFACE_Metadata) == 0) {
    QMetaObject::invokeMethod(
        engine,
        [enginePtr, id]() {
          if (!enginePtr || enginePtr->isShuttingDown()) {
            return;
          }
          enginePtr->bindMetadata(id);
        },
        Qt::QueuedConnection);
    return;
  }

  if (strcmp(type, PW_TYPE_INTERFACE_Node) != 0)
    return;

  if (!props)
    return;

  const char* mediaClass = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
  if (!mediaClass)
    return;

  const bool isSink = strcmp(mediaClass, "Audio/Sink") == 0;
  const bool isSource = strcmp(mediaClass, "Audio/Source") == 0;
  if (!isSink && !isSource)
    return;

  const char* name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
  const char* desc = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);

  const QString qname = QString::fromUtf8(name ? name : "");
  const QString qdesc = QString::fromUtf8(desc ? desc : "");
  const AudioDeviceType devType = isSource ? Source : Sink;

  int cardId = -1;
  if (const char* card = spa_dict_lookup(props, PW_KEY_DEVICE_ID)) {
    bool ok = false;
    cardId = QString::fromUtf8(card).toInt(&ok);
    if (!ok) {
      cardId = -1;
    }
  }

  QString profileName;
  if (const char* profile = spa_dict_lookup(props, "device.profile.name")) {
    profileName = QString::fromUtf8(profile);
  }

  QMetaObject::invokeMethod(
      engine,
      [enginePtr, id, qname, qdesc, devType, cardId, profileName]() {
        if (!enginePtr || enginePtr->isShuttingDown()) {
          return;
        }
        enginePtr->addOrUpdateNode(id, qname, qdesc, devType, cardId, profileName);
      },
      Qt::QueuedConnection);
}

// static
void PipeWireEngine::onRegistryGlobalRemove(void* data, uint32_t id) {
  auto* engine = static_cast<PipeWireEngine*>(data);
  if (!engine)
    return;
  if (engine->isShuttingDown())
    return;

  QPointer<PipeWireEngine> enginePtr(engine);
  QMetaObject::invokeMethod(
      engine,
      [enginePtr, id]() {
        if (!enginePtr || enginePtr->isShuttingDown()) {
          return;
        }
        if (enginePtr->m_metadataId == id) {
          enginePtr->unbindMetadata(id);
          return;
        }
        enginePtr->removeNode(id);
      },
      Qt::QueuedConnection);
}

// static
int PipeWireEngine::onMetadataProperty(void* data,
                                       uint32_t subject,
                                       const char* key,
                                       const char* type,
                                       const char* value) {
  Q_UNUSED(type);

  auto* engine = static_cast<PipeWireEngine*>(data);
  if (!engine || !key)
    return 0;
  if (engine->isShuttingDown())
    return 0;

  if (strcmp(key, "default.audio.sink") == 0 || strcmp(key, "default.audio.source") == 0) {
    const EndpointDirection direction =
        (strcmp(key, "default.audio.sink") == 0) ? EndpointDirection::Output : EndpointDirection::Input;
    const QString nodeName = parseDefaultNodeName(value);

    QPointer<PipeWireEngine> enginePtr(engine);
    QMetaObject::invokeMethod(
        engine,
        [enginePtr, direction, nodeName]() {
          if (!enginePtr || enginePtr->isShuttingDown()) {
            return;
          }
          enginePtr->applyDefaultNodeNameUpdate(direction, nodeName);
        },
        Qt::QueuedConnection);
    return 0;
  }

  if (strcmp(key, "node.disabled") != 0)
    return 0;

  const bool disabled = value && strcmp(value, "true") == 0;
  QPointer<PipeWireEngine> enginePtr(engine);
  QMetaObject::invokeMethod(
      engine,
      [enginePtr, subject, disabled]() {
        if (!enginePtr || enginePtr->isShuttingDown()) {
          return;
        }
        enginePtr->applyNodeDisabledUpdate(subject, disabled);
      },
      Qt::QueuedConnection);
  return 0;
}

void PipeWireEngine::addOrUpdateNode(uint32_t id,
                                     const QString& qname,
                                     const QString& qdesc,
                                     AudioDeviceType type,
                                     int cardId,
                                     const QString& profileName) {
  const bool isBluetooth = qname.contains(QLatin1String("bluez"), Qt::CaseInsensitive);
  qCDebug(lcPipeWireEngine) << "PipeWireEngine: addOrUpdateNode id" << id << "name:" << qname << "desc:" << qdesc
                            << "mediaClass:" << (type == Sink ? "Audio/Sink" : "Audio/Source")
                            << (isBluetooth ? "[Bluetooth]" : "");
  if (isBluetooth) {
    qCDebug(lcVolumeBluetooth) << "PipeWireEngine: discovered Bluetooth node" << qname << "id" << id;
  }

  AudioDevice* dev = m_deviceByWpId.value(id, nullptr);
  const QString previousName = dev ? dev->name() : QString();
  const bool wasSinkListed = dev && m_sinks.contains(dev);
  const bool wasSourceListed = dev && m_sources.contains(dev);
  bool typeChanged = false;

  if (!dev) {
    dev = new AudioDevice(type, this, this);
  }
  else {
    AudioDeviceType oldType = dev->type();
    if (oldType != type) {
      typeChanged = true;
      dev->setType(type);
    }
  }

  dev->setName(qname);
  dev->setIndex(id);
  dev->setDescription(qdesc);
  if (cardId >= 0) {
    dev->setCardId(cardId);
  }
  if (!profileName.isEmpty()) {
    dev->setProfileName(profileName);
  }
  const bool disabled = m_disabledNodeIds.contains(id);
  dev->setEnabled(!disabled);

  m_nodeIdByDevice.insert(dev, id);
  m_deviceByWpId.insert(id, dev);

  if (type == Sink && qname == m_defaultOutputNodeName) {
    setObservedDefaultEndpoint(EndpointDirection::Output, dev);
  }
  else if (type == Source && qname == m_defaultInputNodeName) {
    setObservedDefaultEndpoint(EndpointDirection::Input, dev);
  }

  bindNode(id);

  auto insertSortedByName = [](QList<AudioDevice*>& devices, AudioDevice* device) {
    devices.insert(std::lower_bound(devices.begin(), devices.end(), device,
                                    [](const AudioDevice* left, const AudioDevice* right) {
                                      Q_ASSERT(left != nullptr);
                                      Q_ASSERT(right != nullptr);
                                      return left->name() < right->name();
                                    }),
                   device);
  };

  bool endpointListChanged = false;
  if (type == Sink) {
    if (wasSourceListed) {
      m_sources.removeAll(dev);
      endpointListChanged = true;
    }
    m_sinks.removeAll(dev);
    insertSortedByName(m_sinks, dev);
    endpointListChanged = endpointListChanged || !wasSinkListed || typeChanged || previousName != qname;
  }
  else {
    if (wasSinkListed) {
      m_sinks.removeAll(dev);
      endpointListChanged = true;
    }
    m_sources.removeAll(dev);
    insertSortedByName(m_sources, dev);
    endpointListChanged = endpointListChanged || !wasSourceListed || typeChanged || previousName != qname;
  }

  if (endpointListChanged || type == Sink) {
    emit sinkListChanged();
  }

  // Fetch an initial state snapshot for both output and input nodes so popup
  // controls reflect backend state immediately after discovery.
  queryNodeVolume(id);
}

void PipeWireEngine::removeNode(uint32_t id) {
  AudioDevice* dev = m_deviceByWpId.take(id);
  if (!dev)
    return;

  m_nodeIdByDevice.remove(dev);

  if (dev->type() == Sink && dev->name() == m_defaultOutputNodeName) {
    setObservedDefaultEndpointStableId(EndpointDirection::Output, QString());
  }
  else if (dev->type() == Source && dev->name() == m_defaultInputNodeName) {
    setObservedDefaultEndpointStableId(EndpointDirection::Input, QString());
  }

  bool removedEndpoint = false;
  if (dev->type() == Sink) {
    removedEndpoint = m_sinks.removeAll(dev) > 0;
  }
  else if (dev->type() == Source) {
    removedEndpoint = m_sources.removeAll(dev) > 0;
  }

  if (removedEndpoint) {
    emit sinkListChanged();
  }

  unbindNode(id);
  delete dev;
}

void PipeWireEngine::bindNode(uint32_t id) {
  if (!m_threadLoop || !m_core)
    return;

  pw_thread_loop_lock(m_threadLoop);

  if (m_nodeByNodeId.contains(id)) {
    pw_thread_loop_unlock(m_threadLoop);
    return;
  }

  pw_node* node = static_cast<pw_node*>(pw_registry_bind(m_registry, id, PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, 0));
  if (!node) {
    qCWarning(lcVolumeBackend) << "PipeWireEngine: failed to bind to node" << id;
    pw_thread_loop_unlock(m_threadLoop);
    return;
  }

  static const pw_node_events nodeEvents = {
      PW_VERSION_NODE_EVENTS,
      &PipeWireEngine::onNodeInfo,
      &PipeWireEngine::onNodeParams,
  };

  spa_hook& nodeListener = m_nodeListenerByNodeId[id];
  spa_zero(nodeListener);

  auto* listenerData = new NodeListenerData{this, id};

  pw_node_add_listener(node, &nodeListener, &nodeEvents, listenerData);

  m_nodeByNodeId.insert(id, node);
  m_nodeListenerDataByNodeId.insert(id, listenerData);

  uint32_t ids[] = {SPA_PARAM_Props};
  pw_node_subscribe_params(node, ids, 1);

  pw_node_enum_params(node, 0, SPA_PARAM_Props, 0, UINT32_MAX, nullptr);

  pw_thread_loop_unlock(m_threadLoop);
}

void PipeWireEngine::unbindNode(uint32_t id) {
  if (!m_threadLoop)
    return;

  pw_thread_loop_lock(m_threadLoop);

  pw_node* node = m_nodeByNodeId.value(id, nullptr);
  spa_hook& nodeListener = m_nodeListenerByNodeId[id];
  auto* listenerData = static_cast<NodeListenerData*>(m_nodeListenerDataByNodeId.value(id, nullptr));

  if (node) {
    if (nodeListener.link.next || nodeListener.link.prev) {
      spa_hook_remove(&nodeListener);
    }
    pw_proxy_destroy(reinterpret_cast<pw_proxy*>(node));
    m_nodeByNodeId.remove(id);
  }

  if (listenerData) {
    delete listenerData;
    m_nodeListenerDataByNodeId.remove(id);
  }

  m_nodeListenerByNodeId.remove(id);

  pw_thread_loop_unlock(m_threadLoop);
}

void PipeWireEngine::bindMetadata(uint32_t id) {
  if (!m_threadLoop || !m_registry)
    return;

  pw_thread_loop_lock(m_threadLoop);

  if (m_metadata) {
    pw_thread_loop_unlock(m_threadLoop);
    return;
  }

  pw_metadata* metadata =
      static_cast<pw_metadata*>(pw_registry_bind(m_registry, id, PW_TYPE_INTERFACE_Metadata, PW_VERSION_METADATA, 0));
  if (!metadata) {
    qCWarning(lcVolumeBackend) << "PipeWireEngine: failed to bind metadata object" << id;
    pw_thread_loop_unlock(m_threadLoop);
    return;
  }

  static const pw_metadata_events metadataEvents = {
      PW_VERSION_METADATA_EVENTS,
      &PipeWireEngine::onMetadataProperty,
  };

  spa_zero(m_metadataListener);
  pw_metadata_add_listener(metadata, &m_metadataListener, &metadataEvents, this);

  m_metadata = metadata;
  m_metadataId = id;

  pw_thread_loop_unlock(m_threadLoop);
}

void PipeWireEngine::unbindMetadata(uint32_t id) {
  if (!m_threadLoop || m_metadataId != id)
    return;

  pw_thread_loop_lock(m_threadLoop);

  if (m_metadata) {
    spa_hook_remove(&m_metadataListener);
    pw_proxy_destroy(reinterpret_cast<pw_proxy*>(m_metadata));
    m_metadata = nullptr;
  }

  spa_zero(m_metadataListener);
  m_metadataId = SPA_ID_INVALID;
  m_disabledNodeIds.clear();

  pw_thread_loop_unlock(m_threadLoop);
}

// static
void PipeWireEngine::onNodeInfo(void* data, const pw_node_info* info) {
  Q_UNUSED(data);
  Q_UNUSED(info);
}

// static
void PipeWireEngine::onNodeParams(void* data,
                                  int seq,
                                  uint32_t paramId,
                                  uint32_t index,
                                  uint32_t next,
                                  const spa_pod* param) {
  Q_UNUSED(seq);
  Q_UNUSED(index);
  Q_UNUSED(next);
  // paramId used in debug logging below

  auto* ctx = static_cast<NodeListenerData*>(data);
  if (!ctx || !ctx->engine || !param)
    return;

  PipeWireEngine* engine = ctx->engine;
  uint32_t nodeId = ctx->nodeId;
  if (engine->isShuttingDown()) {
    return;
  }

  if (param->type != SPA_TYPE_Object) {
    return;
  }

  const spa_pod_object* obj = reinterpret_cast<const spa_pod_object*>(param);
  if (obj->body.type != SPA_TYPE_OBJECT_Props) {
    return;
  }

  auto* mutableObj = reinterpret_cast<spa_pod_object*>(const_cast<spa_pod*>(param));
  spa_pod_prop* prop;

  bool hasVolume = false;
  float volume = 0.0f;
  bool hasMute = false;
  bool mute = false;

  SPA_POD_OBJECT_FOREACH(mutableObj, prop) {
    switch (prop->key) {
      case SPA_PROP_volume: {
        float v = 0.0f;
        if (spa_pod_get_float(&prop->value, &v) == 0) {
          hasVolume = true;
          volume = v;
        }
        break;
      }
      case SPA_PROP_mute: {
        bool m = false;
        if (spa_pod_get_bool(&prop->value, &m) == 0) {
          hasMute = true;
          mute = m;
        }
        break;
      }
    }
  }

  if (!hasVolume && !hasMute) {
    return;
  }

  QPointer<PipeWireEngine> enginePtr(engine);
  QMetaObject::invokeMethod(
      engine,
      [enginePtr, nodeId, paramId, hasVolume, volume, hasMute, mute]() {
        if (!enginePtr || enginePtr->isShuttingDown()) {
          return;
        }
        enginePtr->applyNodeParamUpdate(nodeId, hasVolume, volume, hasMute, mute);

        // Debug logging for node param updates
        const AudioDevice* dev = enginePtr->m_deviceByWpId.value(nodeId, nullptr);
        if (!dev) {
          return;
        }
        const bool isBluetooth = dev->name().contains(QLatin1String("bluez"), Qt::CaseInsensitive);
        qCDebug(lcPipeWireEngine) << "PipeWireEngine: onNodeParams node" << nodeId << "device" << dev->name()
                                  << (isBluetooth ? "[Bluetooth]" : "") << "paramId" << paramId;
      },
      Qt::QueuedConnection);
}

void PipeWireEngine::applyNodeParamUpdate(uint32_t nodeId, bool hasVolume, float volume, bool hasMute, bool mute) {
  AudioDevice* device = m_deviceByWpId.value(nodeId, nullptr);
  if (!device) {
    return;
  }

  if (hasVolume) {
    int percent = static_cast<int>(volume * 100.0f);
    percent = std::clamp(percent, 0, 100);
    device->setVolumeNoCommit(percent);
  }

  if (hasMute) {
    device->setMuteNoCommit(mute);
  }
}

void PipeWireEngine::applyNodeDisabledUpdate(uint32_t nodeId, bool disabled) {
  if (disabled) {
    m_disabledNodeIds.insert(nodeId);
  }
  else {
    m_disabledNodeIds.remove(nodeId);
  }

  AudioDevice* dev = m_deviceByWpId.value(nodeId, nullptr);
  if (dev) {
    setNodeEnabledState(dev, !disabled);
  }
}

void PipeWireEngine::applyDefaultNodeNameUpdate(AudioEngine::EndpointDirection direction, const QString& nodeName) {
  const QString normalizedNodeName = nodeName.trimmed();
  if (direction == EndpointDirection::Output) {
    m_defaultOutputNodeName = normalizedNodeName;
  }
  else {
    m_defaultInputNodeName = normalizedNodeName;
  }

  AudioDevice* matchedDevice = nullptr;
  for (AudioDevice* device : std::as_const(m_deviceByWpId)) {
    if (!device || device->name() != normalizedNodeName) {
      continue;
    }

    if (direction == EndpointDirection::Output && device->type() != Sink) {
      continue;
    }
    if (direction == EndpointDirection::Input && device->type() != Source) {
      continue;
    }

    matchedDevice = device;
    break;
  }

  setObservedDefaultEndpoint(direction, matchedDevice);
}

void PipeWireEngine::queryNodeVolume(uint32_t nodeId) {
  if (!m_threadLoop)
    return;

  pw_thread_loop_lock(m_threadLoop);

  pw_node* node = m_nodeByNodeId.value(nodeId, nullptr);
  if (!node) {
    pw_thread_loop_unlock(m_threadLoop);
    return;
  }

  int res = pw_node_enum_params(node, 0, SPA_PARAM_Props, 0, UINT32_MAX, nullptr);
  if (res < 0) {
    qCWarning(lcVolumeBackend) << "PipeWireEngine: failed to enum params for node" << nodeId << "error:" << res;
  }
  else {
    qCDebug(lcPipeWireEngine) << "PipeWireEngine: queried volume/mute for node" << nodeId;
  }

  pw_thread_loop_unlock(m_threadLoop);
}

bool PipeWireEngine::setNodeVolume(uint32_t nodeId, float volume) {
  if (!m_threadLoop)
    return false;

  pw_thread_loop_lock(m_threadLoop);

  pw_node* node = m_nodeByNodeId.value(nodeId, nullptr);
  if (!node) {
    pw_thread_loop_unlock(m_threadLoop);
    return false;
  }

  volume = std::clamp(volume, 0.0f, 1.0f);

  uint8_t buffer[1024];
  spa_pod_builder builder;
  spa_pod_builder_init(&builder, buffer, sizeof(buffer));

  spa_pod_frame frame;
  spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props);
  spa_pod_builder_prop(&builder, SPA_PROP_volume, 0);
  spa_pod_builder_float(&builder, volume);
  spa_pod* param = reinterpret_cast<spa_pod*>(spa_pod_builder_pop(&builder, &frame));

  bool success = false;
  if (param) {
    int res = pw_node_set_param(node, SPA_PARAM_Props, 0, param);
    if (res < 0) {
      qCWarning(lcVolumeBackend) << "PipeWireEngine: failed to set volume for node" << nodeId << "error:" << res;
    }
    else {
      // Find device for logging
      QString devName;
      bool isBluetooth = false;
      for (AudioDevice* dev : std::as_const(m_sinks)) {
        if (m_nodeIdByDevice.value(dev) == nodeId) {
          devName = dev->name();
          isBluetooth = devName.contains(QLatin1String("bluez"), Qt::CaseInsensitive);
          break;
        }
      }
      qCDebug(lcPipeWireEngine) << "PipeWireEngine: set volume for node" << nodeId << "device" << devName
                                << (isBluetooth ? "[Bluetooth]" : "") << "to" << volume;
      qCDebug(lcVolumeRouting) << "PipeWireEngine: routed volume update to node" << nodeId << "device" << devName
                               << "volume" << volume;
      success = true;
    }
  }

  pw_thread_loop_unlock(m_threadLoop);
  return success;
}

bool PipeWireEngine::setNodeMute(uint32_t nodeId, bool mute) {
  if (!m_threadLoop)
    return false;

  pw_thread_loop_lock(m_threadLoop);

  pw_node* node = m_nodeByNodeId.value(nodeId, nullptr);
  if (!node) {
    pw_thread_loop_unlock(m_threadLoop);
    return false;
  }

  uint8_t buffer[1024];
  spa_pod_builder builder;
  spa_pod_builder_init(&builder, buffer, sizeof(buffer));

  spa_pod_frame frame;
  spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props);
  spa_pod_builder_prop(&builder, SPA_PROP_mute, 0);
  spa_pod_builder_bool(&builder, mute);
  spa_pod* param = reinterpret_cast<spa_pod*>(spa_pod_builder_pop(&builder, &frame));

  bool success = false;
  if (param) {
    int res = pw_node_set_param(node, SPA_PARAM_Props, 0, param);
    if (res < 0) {
      qCWarning(lcVolumeBackend) << "PipeWireEngine: failed to set mute for node" << nodeId << "error:" << res;
    }
    else {
      // Find device for logging
      QString devName;
      bool isBluetooth = false;
      for (AudioDevice* dev : std::as_const(m_sinks)) {
        if (m_nodeIdByDevice.value(dev) == nodeId) {
          devName = dev->name();
          isBluetooth = devName.contains(QLatin1String("bluez"), Qt::CaseInsensitive);
          break;
        }
      }
      qCDebug(lcPipeWireEngine) << "PipeWireEngine: set mute for node" << nodeId << "device" << devName
                                << (isBluetooth ? "[Bluetooth]" : "") << "to" << mute;
      if (isBluetooth) {
        qCDebug(lcVolumeBluetooth) << "PipeWireEngine: applied Bluetooth mute update for node" << nodeId << "device"
                                   << devName << "mute" << mute;
      }
      else {
        qCDebug(lcVolumeRouting) << "PipeWireEngine: applied mute update for node" << nodeId << "device" << devName
                                 << "mute" << mute;
      }
      success = true;
    }
  }

  pw_thread_loop_unlock(m_threadLoop);
  return success;
}

bool PipeWireEngine::setNodeDisabledMetadata(uint32_t nodeId, bool disabled) {
  if (!m_threadLoop || !m_metadata)
    return false;

  pw_thread_loop_lock(m_threadLoop);
  const int res =
      pw_metadata_set_property(m_metadata, nodeId, "node.disabled", "Spa:Bool", disabled ? "true" : "false");
  if (res < 0) {
    qCWarning(lcVolumeBackend) << "PipeWireEngine: failed to set node.disabled for" << nodeId << "error" << res;
    pw_thread_loop_unlock(m_threadLoop);
    return false;
  }

  if (disabled)
    m_disabledNodeIds.insert(nodeId);
  else
    m_disabledNodeIds.remove(nodeId);

  pw_thread_loop_unlock(m_threadLoop);
  return true;
}

void PipeWireEngine::setNodeEnabledState(AudioDevice* dev, bool enabled) {
  if (!dev)
    return;
  dev->setEnabled(enabled);
}

bool PipeWireEngine::commitDeviceVolume(AudioDevice* device) {
  if (!device || !m_ready || !m_threadLoop)
    return false;

  uint32_t nodeId = m_nodeIdByDevice.value(device, SPA_ID_INVALID);
  if (nodeId == SPA_ID_INVALID)
    return false;

  int percent = device->volume();
  if (percent < 0)
    percent = 0;
  if (percent > m_maximumVolume)
    percent = m_maximumVolume;

  float volume = static_cast<float>(percent) / 100.0f;
  if (volume > 1.0f)
    volume = 1.0f;

  const QString devName = device->name();
  const bool isBluetooth = devName.contains(QLatin1String("bluez"), Qt::CaseInsensitive);
  qCDebug(lcPipeWireEngine) << "PipeWireEngine: commitDeviceVolume device" << devName
                            << (isBluetooth ? "[Bluetooth]" : "") << "node" << nodeId << "volume" << percent
                            << "% (" << volume << ")";
  if (isBluetooth) {
    qCDebug(lcVolumeBluetooth) << "PipeWireEngine: committing Bluetooth volume for" << devName << "node" << nodeId
                               << "percent" << percent;
  }
  else {
    qCDebug(lcVolumeRouting) << "PipeWireEngine: committing volume for" << devName << "node" << nodeId << "percent"
                             << percent;
  }

  return setNodeVolume(nodeId, volume);
}

bool PipeWireEngine::setMute(AudioDevice* device, bool state) {
  if (!device || !m_ready || !m_threadLoop)
    return false;

  uint32_t nodeId = m_nodeIdByDevice.value(device, SPA_ID_INVALID);
  if (nodeId == SPA_ID_INVALID)
    return false;

  const QString devName = device->name();
  const bool isBluetooth = devName.contains(QLatin1String("bluez"), Qt::CaseInsensitive);
  qCDebug(lcPipeWireEngine) << "PipeWireEngine: setMute device" << devName << (isBluetooth ? "[Bluetooth]" : "")
                            << "node" << nodeId << "mute" << state;
  if (isBluetooth) {
    qCDebug(lcVolumeBluetooth) << "PipeWireEngine: committing Bluetooth mute for" << devName << "node" << nodeId
                               << "mute" << state;
  }
  else {
    qCDebug(lcVolumeRouting) << "PipeWireEngine: committing mute for" << devName << "node" << nodeId << "mute"
                             << state;
  }

  return setNodeMute(nodeId, state);
}

void PipeWireEngine::setIgnoreMaxVolume(bool ignore) {
  AudioEngine::setIgnoreMaxVolume(ignore);
  m_maximumVolume = ignore ? 150 : 100;
}
