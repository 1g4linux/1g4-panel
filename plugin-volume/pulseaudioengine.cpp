/* plugin-volume/pulseaudioengine.cpp
 * Volume control plugin implementation
 */

#include "pulseaudioengine.h"

#include "audiodevice.h"
#include "volumelogging.h"

#include <QMetaObject>
#include <QMetaType>
#include <QPointer>
#include <QtDebug>

#include <cmath>
#include <algorithm>
#include <memory>

// #define PULSEAUDIO_ENGINE_DEBUG

static void sinkInfoCallback(pa_context* context, const pa_sink_info* info, int isLast, void* userdata) {
  auto* pulseEngine = static_cast<PulseAudioEngine*>(userdata);

  if (isLast < 0) {
    pa_threaded_mainloop_signal(pulseEngine->mainloop(), 0);
    qCWarning(lcVolumeBackend) << QStringLiteral("Failed to get sink information: %1")
                                      .arg(QString::fromUtf8(pa_strerror(pa_context_errno(context))));
    return;
  }

  if (isLast) {
    pa_threaded_mainloop_signal(pulseEngine->mainloop(), 0);
    return;
  }

  // Callbacks are executed from PulseAudio's internal thread. Queue the UI-model update onto
  // the engine's Qt thread to avoid cross-thread access to QObjects and m_sinks.
  const QString name = QString::fromUtf8(info->name ? info->name : "");
  const QString description = QString::fromUtf8(info->description ? info->description : "");
  const uint32_t index = info->index;
  const bool mute = info->mute;
  const pa_cvolume cvolume = info->volume;

  QPointer<PulseAudioEngine> enginePtr(pulseEngine);
  QMetaObject::invokeMethod(
      pulseEngine,
      [enginePtr, name, index, description, mute, cvolume]() {
        if (!enginePtr) {
          return;
        }
        enginePtr->addOrUpdateSinkSnapshot(name, index, description, mute, cvolume);
      },
      Qt::QueuedConnection);
}

static void contextEventCallback(pa_context* context, const char* name, pa_proplist* p, void* userdata) {
  Q_UNUSED(context);
  Q_UNUSED(p);
  Q_UNUSED(userdata);

#ifdef PULSEAUDIO_ENGINE_DEBUG
  qCDebug(lcVolumeBackend) << "PulseAudioEngine: event received" << name;
#else
  Q_UNUSED(name);
#endif
}

static void contextStateCallback(pa_context* context, void* userdata) {
  auto* pulseEngine = static_cast<PulseAudioEngine*>(userdata);

  // update internal state
  const pa_context_state_t state = pa_context_get_state(context);
  pulseEngine->setContextState(state);

#ifdef PULSEAUDIO_ENGINE_DEBUG
  switch (state) {
    case PA_CONTEXT_UNCONNECTED:
      qCDebug(lcVolumeBackend) << "PulseAudioEngine: context unconnected";
      break;
    case PA_CONTEXT_CONNECTING:
      qCDebug(lcVolumeBackend) << "PulseAudioEngine: context connecting";
      break;
    case PA_CONTEXT_AUTHORIZING:
      qCDebug(lcVolumeBackend) << "PulseAudioEngine: context authorizing";
      break;
    case PA_CONTEXT_SETTING_NAME:
      qCDebug(lcVolumeBackend) << "PulseAudioEngine: context setting name";
      break;
    case PA_CONTEXT_READY:
      qCDebug(lcVolumeBackend) << "PulseAudioEngine: context ready";
      break;
    case PA_CONTEXT_FAILED:
      qCDebug(lcVolumeBackend) << "PulseAudioEngine: context failed";
      break;
    case PA_CONTEXT_TERMINATED:
      qCDebug(lcVolumeBackend) << "PulseAudioEngine: context terminated";
      break;
    default:
      qCDebug(lcVolumeBackend) << "PulseAudioEngine: unexpected context state";
  }
#endif

  pa_threaded_mainloop_signal(pulseEngine->mainloop(), 0);
}

static void contextSuccessCallback(pa_context* context, int success, void* userdata) {
  Q_UNUSED(context);
  Q_UNUSED(success);

  auto* pulseEngine = static_cast<PulseAudioEngine*>(userdata);
  pa_threaded_mainloop_signal(pulseEngine->mainloop(), 0);
}

static void contextSubscriptionCallback(pa_context* context,
                                        pa_subscription_event_type_t t,
                                        uint32_t idx,
                                        void* userdata) {
  Q_UNUSED(context);

  auto* pulseEngine = static_cast<PulseAudioEngine*>(userdata);
  const auto eventType = static_cast<pa_subscription_event_type_t>(t & PA_SUBSCRIPTION_EVENT_TYPE_MASK);
  if (eventType == PA_SUBSCRIPTION_EVENT_REMOVE) {
    QPointer<PulseAudioEngine> enginePtr(pulseEngine);
    QMetaObject::invokeMethod(
        pulseEngine,
        [enginePtr, idx]() {
          if (!enginePtr) {
            return;
          }
          enginePtr->removeSink(idx);
        },
        Qt::QueuedConnection);
  }
  else {
    pulseEngine->requestSinkInfoUpdate(idx);
  }
}

PulseAudioEngine::PulseAudioEngine(QObject* parent)
    : AudioEngine(parent),
      m_context(nullptr),
      m_contextState(PA_CONTEXT_UNCONNECTED),
      m_ready(false),
      m_maximumVolume(PA_VOLUME_NORM) {
  qRegisterMetaType<pa_context_state_t>("pa_context_state_t");

  m_reconnectionTimer.setSingleShot(true);
  m_reconnectionTimer.setInterval(100);
  connect(&m_reconnectionTimer, &QTimer::timeout, this, &PulseAudioEngine::connectContext);

  m_mainLoop = pa_threaded_mainloop_new();
  if (m_mainLoop == nullptr) {
    qCWarning(lcVolumeBackend) << "PulseAudioEngine: unable to create PulseAudio mainloop";
    return;
  }

  if (pa_threaded_mainloop_start(m_mainLoop) != 0) {
    qCWarning(lcVolumeBackend) << "PulseAudioEngine: unable to start PulseAudio mainloop";
    pa_threaded_mainloop_free(m_mainLoop);
    m_mainLoop = nullptr;
    return;
  }

  m_mainLoopApi = pa_threaded_mainloop_get_api(m_mainLoop);

  connect(this, &PulseAudioEngine::contextStateChanged, this, &PulseAudioEngine::handleContextStateChanged);

  connectContext();
}

PulseAudioEngine::~PulseAudioEngine() {
  if (m_context) {
    pa_context_unref(m_context);
    m_context = nullptr;
  }

  if (m_mainLoop) {
    pa_threaded_mainloop_stop(m_mainLoop);
    pa_threaded_mainloop_free(m_mainLoop);
    m_mainLoop = nullptr;
  }
}

void PulseAudioEngine::removeSink(uint32_t idx) {
  auto dev_i = std::find_if(m_sinks.begin(), m_sinks.end(), [idx](AudioDevice* dev) { return dev->index() == idx; });
  if (dev_i == m_sinks.end())
    return;

  std::unique_ptr<AudioDevice> dev{*dev_i};
  m_cVolumeMap.remove(dev.get());
  m_sinks.erase(dev_i);
  emit sinkListChanged();
}

void PulseAudioEngine::addOrUpdateSink(const pa_sink_info* info) {
  AudioDevice* dev = nullptr;
  bool newSink = false;
  const QString name = QString::fromUtf8(info->name);

  for (AudioDevice* device : std::as_const(m_sinks)) {
    if (device->name() == name) {
      dev = device;
      break;
    }
  }

  if (!dev) {
    dev = new AudioDevice(Sink, this);
    newSink = true;
  }

  dev->setName(name);
  dev->setIndex(info->index);
  dev->setDescription(QString::fromUtf8(info->description));
  dev->setMuteNoCommit(info->mute);

  // TODO: save separately? alsa does not have it
  m_cVolumeMap.insert(dev, info->volume);

  const pa_volume_t v = pa_cvolume_avg(&(info->volume));
  // convert real volume to percentage
  dev->setVolumeNoCommit(std::round((static_cast<double>(v) * 100.0) / m_maximumVolume));

  if (newSink) {
    // keep the sinks sorted by name
    m_sinks.insert(std::lower_bound(m_sinks.begin(), m_sinks.end(), dev,
                                    [](const AudioDevice* a, const AudioDevice* b) { return a->name() < b->name(); }),
                   dev);
    emit sinkListChanged();
  }
}

void PulseAudioEngine::addOrUpdateSinkSnapshot(const QString& name,
                                               uint32_t index,
                                               const QString& description,
                                               bool mute,
                                               pa_cvolume cvolume) {
  AudioDevice* dev = nullptr;
  bool newSink = false;

  for (AudioDevice* device : std::as_const(m_sinks)) {
    if (device->name() == name) {
      dev = device;
      break;
    }
  }

  if (!dev) {
    dev = new AudioDevice(Sink, this);
    newSink = true;
  }

  dev->setName(name);
  dev->setIndex(index);
  dev->setDescription(description);
  dev->setMuteNoCommit(mute);

  // TODO: save separately? alsa does not have it
  m_cVolumeMap.insert(dev, cvolume);

  const pa_volume_t v = pa_cvolume_avg(&cvolume);
  // convert real volume to percentage
  dev->setVolumeNoCommit(std::round((static_cast<double>(v) * 100.0) / m_maximumVolume));

  if (newSink) {
    // keep the sinks sorted by name
    m_sinks.insert(std::lower_bound(m_sinks.begin(), m_sinks.end(), dev,
                                    [](const AudioDevice* a, const AudioDevice* b) { return a->name() < b->name(); }),
                   dev);
    emit sinkListChanged();
  }
}

void PulseAudioEngine::requestSinkInfoUpdate(uint32_t idx) {
  emit sinkInfoChanged(idx);
}

void PulseAudioEngine::commitDeviceVolume(AudioDevice* device) {
  if (!device || !m_ready)
    return;

  // convert from percentage to real volume value
  const auto v = static_cast<pa_volume_t>((static_cast<double>(device->volume()) / 100.0) * m_maximumVolume);
  pa_cvolume tmpVolume = m_cVolumeMap.value(device);
  pa_cvolume* volume = pa_cvolume_set(&tmpVolume, tmpVolume.channels, v);
  // qDebug() << "PulseAudioEngine::commitDeviceVolume" << v;

  pa_threaded_mainloop_lock(m_mainLoop);

  pa_operation* operation = nullptr;
  if (device->type() == Sink)
    operation = pa_context_set_sink_volume_by_index(m_context, device->index(), volume, contextSuccessCallback, this);
  else
    operation = pa_context_set_source_volume_by_index(m_context, device->index(), volume, contextSuccessCallback, this);

  if (operation) {
    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
      pa_threaded_mainloop_wait(m_mainLoop);
    pa_operation_unref(operation);
  }

  pa_threaded_mainloop_unlock(m_mainLoop);
}

void PulseAudioEngine::retrieveSinks() {
  if (!m_ready)
    return;

  pa_threaded_mainloop_lock(m_mainLoop);

  pa_operation* operation = pa_context_get_sink_info_list(m_context, sinkInfoCallback, this);
  if (operation) {
    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
      pa_threaded_mainloop_wait(m_mainLoop);
    pa_operation_unref(operation);
  }

  pa_threaded_mainloop_unlock(m_mainLoop);
}

void PulseAudioEngine::setupSubscription() {
  if (!m_ready)
    return;

  connect(this, &PulseAudioEngine::sinkInfoChanged, this, &PulseAudioEngine::retrieveSinkInfo,
          static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));
  pa_context_set_subscribe_callback(m_context, contextSubscriptionCallback, this);

  pa_threaded_mainloop_lock(m_mainLoop);

  pa_operation* operation = pa_context_subscribe(m_context, PA_SUBSCRIPTION_MASK_SINK, contextSuccessCallback, this);
  if (operation) {
    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
      pa_threaded_mainloop_wait(m_mainLoop);
    pa_operation_unref(operation);
  }

  pa_threaded_mainloop_unlock(m_mainLoop);
}

void PulseAudioEngine::handleContextStateChanged() {
  if (m_contextState == PA_CONTEXT_FAILED || m_contextState == PA_CONTEXT_TERMINATED) {
    qCWarning(lcVolumeBackend)
        << "PulseAudioEngine: context failed or terminated, scheduling reconnection";
    m_reconnectionTimer.start();
  }
}

void PulseAudioEngine::connectContext() {
  bool keepGoing = true;
  bool ok = false;

  m_reconnectionTimer.stop();

  if (!m_mainLoop)
    return;

  pa_threaded_mainloop_lock(m_mainLoop);

  if (m_context) {
    pa_context_unref(m_context);
    m_context = nullptr;
  }

  m_context = pa_context_new(m_mainLoopApi, "oneg4-volume");
  pa_context_set_state_callback(m_context, contextStateCallback, this);
  pa_context_set_event_callback(m_context, contextEventCallback, this);

  if (!m_context) {
    pa_threaded_mainloop_unlock(m_mainLoop);
    m_reconnectionTimer.start();
    return;
  }

  if (pa_context_connect(m_context, nullptr, static_cast<pa_context_flags_t>(0), nullptr) < 0) {
    pa_threaded_mainloop_unlock(m_mainLoop);
    m_reconnectionTimer.start();
    return;
  }

  while (keepGoing) {
    switch (m_contextState) {
      case PA_CONTEXT_CONNECTING:
      case PA_CONTEXT_AUTHORIZING:
      case PA_CONTEXT_SETTING_NAME:
        break;

      case PA_CONTEXT_READY:
        keepGoing = false;
        ok = true;
        break;

      case PA_CONTEXT_TERMINATED:
        keepGoing = false;
        break;

      case PA_CONTEXT_FAILED:
      default:
        qCWarning(lcVolumeBackend) << QStringLiteral("Connection failure: %1")
                                          .arg(QString::fromUtf8(pa_strerror(pa_context_errno(m_context))));
        keepGoing = false;
        break;
    }

    if (keepGoing)
      pa_threaded_mainloop_wait(m_mainLoop);
  }

  pa_threaded_mainloop_unlock(m_mainLoop);

  if (ok) {
    retrieveSinks();
    setupSubscription();
  }
  else {
    m_reconnectionTimer.start();
  }
}

void PulseAudioEngine::retrieveSinkInfo(uint32_t idx) {
  if (!m_ready)
    return;

  pa_threaded_mainloop_lock(m_mainLoop);

  pa_operation* operation = pa_context_get_sink_info_by_index(m_context, idx, sinkInfoCallback, this);
  if (operation) {
    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
      pa_threaded_mainloop_wait(m_mainLoop);
    pa_operation_unref(operation);
  }

  pa_threaded_mainloop_unlock(m_mainLoop);
}

void PulseAudioEngine::setMute(AudioDevice* device, bool state) {
  if (!m_ready || !device)
    return;

  pa_threaded_mainloop_lock(m_mainLoop);

  pa_operation* operation =
      pa_context_set_sink_mute_by_index(m_context, device->index(), state, contextSuccessCallback, this);
  if (operation) {
    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
      pa_threaded_mainloop_wait(m_mainLoop);
    pa_operation_unref(operation);
  }

  pa_threaded_mainloop_unlock(m_mainLoop);
}

void PulseAudioEngine::setContextState(pa_context_state_t state) {
  if (m_contextState == state)
    return;

  m_contextState = state;

  // update ready member as it depends on state
  const bool newReady = (m_contextState == PA_CONTEXT_READY);
  if (m_ready == newReady)
    return;

  m_ready = newReady;

  emit contextStateChanged(m_contextState);
  emit readyChanged(m_ready);
}

void PulseAudioEngine::setIgnoreMaxVolume(bool ignore) {
  const int oldMax = m_maximumVolume;
  if (ignore)
    m_maximumVolume = PA_VOLUME_UI_MAX;
  else
    m_maximumVolume = PA_VOLUME_NORM;
  if (oldMax != m_maximumVolume)
    retrieveSinks();
}
