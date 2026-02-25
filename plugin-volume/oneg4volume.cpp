/* plugin-volume/oneg4volume.cpp
 * Volume control plugin implementation
 */

#include "oneg4volume.h"

#include "audiodevice.h"
#include "audioengine.h"
#include "oneg4volumeconfiguration.h"
#include "sinkselection.h"
#include "volumelogging.h"
#ifdef USE_PIPEWIRE
#include "pipewireengine.h"
#endif
#ifdef ONEG4_VOLUME_DEV_TEST_BACKENDS
#include "testaudioengine.h"
#endif
#include "volumebutton.h"
#include "volumepopup.h"

#include <OneG4/Notification.h>
#include <QDialog>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QMessageBox>
#include <QPointer>
#include <QThread>
#include <QtGlobal>

#include <algorithm>

QDialog* create_1g4_mixer_dialog();

OneG4Volume::OneG4Volume(const IOneG4PanelPluginStartupInfo& startupInfo)
    : QObject(),
      IOneG4PanelPlugin(startupInfo),
      m_engine(nullptr),
      m_defaultSinkId(0U),
      m_defaultSourceId(0U),
      m_defaultSink(nullptr),
      m_defaultSource(nullptr),
      m_configDialog(nullptr),
      m_mixerDialog(nullptr),
      m_alwaysShowNotifications(SETTINGS_DEFAULT_ALWAYS_SHOW_NOTIFICATIONS) {
  m_volumeButton = new VolumeButton(this);

  m_notification = new OneG4::Notification(QString(), this);
  connect(m_volumeButton->volumePopup(), &VolumePopup::externalMixerRequested, this, &OneG4Volume::openExternalMixer);

#ifdef ONEG4_VOLUME_DEV_VERBOSE_LOGGING
  QLoggingCategory::setFilterRules(QStringLiteral(
      "oneg4.panel.plugin.volume.ui.debug=true\n"
      "oneg4.panel.plugin.volume.backend.debug=true\n"
      "oneg4.panel.plugin.volume.bluetooth.debug=true\n"
      "oneg4.panel.plugin.volume.routing.debug=true\n"
      "oneg4.panel.plugin.volume.persistence.debug=true\n"
      "oneg4.panel.plugin.volume.pipewire.debug=true\n"));
#endif

#ifdef ONEG4_VOLUME_ENABLE_BLUETOOTH_BATTERY
  qCDebug(lcVolumeBluetooth) << "OneG4Volume: Bluetooth battery integration enabled";
#else
  qCDebug(lcVolumeBluetooth) << "OneG4Volume: Bluetooth battery integration disabled";
#endif

  settingsChanged();
}

OneG4Volume::~OneG4Volume() {
  setDefaultSource(nullptr);
  setDefaultSink(nullptr);

  if (m_engine) {
    disconnect(m_engine, nullptr, this, nullptr);
    delete m_engine;
    m_engine = nullptr;
  }

  delete m_mixerDialog;
  m_mixerDialog = nullptr;

  delete m_volumeButton;
  m_volumeButton = nullptr;
}

void OneG4Volume::setAudioEngine(AudioEngine* engine) {
  if (!engine) {
    return;
  }

  if (engine == m_engine) {
    return;
  }

  if (m_engine && m_engine->backendName() == engine->backendName()) {
    delete engine;
    return;
  }

  if (m_engine) {
    setDefaultSource(nullptr);
    setDefaultSink(nullptr);

    disconnect(m_engine, nullptr, this, nullptr);
    delete m_engine;
    m_engine = nullptr;
  }

  if (engine->parent() != this) {
    engine->setParent(this);
  }

  m_engine = engine;

  connect(m_engine, &AudioEngine::sinkListChanged, this, &OneG4Volume::handleSinkListChanged, Qt::QueuedConnection);
  connect(m_engine, &AudioEngine::stateChanged, this, &OneG4Volume::handleEngineStateChanged, Qt::QueuedConnection);

  handleSinkListChanged();
  handleEngineStateChanged();
}

void OneG4Volume::settingsChanged() {
  m_defaultSinkId =
      static_cast<uint>(std::max(settings()->value(QStringLiteral(SETTINGS_DEVICE), SETTINGS_DEFAULT_DEVICE).toInt(), 0));

  const QString engineName =
      settings()
          ->value(QStringLiteral(SETTINGS_AUDIO_ENGINE), QStringLiteral(SETTINGS_DEFAULT_AUDIO_ENGINE))
          .toString();

  const bool newEngine = !m_engine || m_engine->backendName() != engineName;
  if (newEngine) {
    AudioEngine* engine = nullptr;
    if (engineName == QLatin1String("PipeWire")) {
#ifdef USE_PIPEWIRE
      engine = new PipeWireEngine(this);
#endif
    }
#ifdef ONEG4_VOLUME_DEV_TEST_BACKENDS
    else if (engineName == QLatin1String("TestBackend")) {
      engine = new TestAudioEngine(this);
    }
#endif
    if (!engine) {
      // PipeWire is the only runtime backend in production builds.
#ifdef USE_PIPEWIRE
      engine = new PipeWireEngine(this);
#endif
    }
    if (engine) {
      if (engineName != engine->backendName()) {
        settings()->setValue(QStringLiteral(SETTINGS_AUDIO_ENGINE), engine->backendName());
      }
      setAudioEngine(engine);
    }
  }

  m_volumeButton->setMuteOnMiddleClick(
      settings()->value(QStringLiteral(SETTINGS_MUTE_ON_MIDDLECLICK), SETTINGS_DEFAULT_MUTE_ON_MIDDLECLICK).toBool());

  m_volumeButton->volumePopup()->setSliderStep(
      settings()->value(QStringLiteral(SETTINGS_STEP), SETTINGS_DEFAULT_STEP).toInt());

  m_alwaysShowNotifications =
      settings()
          ->value(QStringLiteral(SETTINGS_ALWAYS_SHOW_NOTIFICATIONS), SETTINGS_DEFAULT_ALWAYS_SHOW_NOTIFICATIONS)
          .toBool();

  if (!newEngine) {
    handleSinkListChanged();
  }
}

void OneG4Volume::handleSinkListChanged() {
  if (QThread::currentThread() != thread()) {
    QPointer<OneG4Volume> self(this);
    QMetaObject::invokeMethod(
        this,
        [self]() {
          if (!self) {
            return;
          }
          self->handleSinkListChanged();
        },
        Qt::QueuedConnection);
    return;
  }

  if (!m_engine) {
    return;
  }

  const auto& sinks = m_engine->sinks();
  if (!sinks.isEmpty()) {
    QList<uint> sinkIds;
    sinkIds.reserve(sinks.size());
    for (const AudioDevice* sink : sinks) {
      sinkIds.append(sink->index());
    }

    const QVariant storedSinkSetting = settings()->value(QStringLiteral(SETTINGS_DEVICE), SETTINGS_DEFAULT_DEVICE);
    const bool migrationDone = settings()->value(QStringLiteral(SETTINGS_DEVICE_ID_MIGRATION_DONE), false).toBool();
    if (const std::optional<uint> migratedSinkId = migrateLegacySinkSelection(sinkIds, storedSinkSetting, migrationDone);
        migratedSinkId.has_value()) {
      const QVariant migratedValue = static_cast<uint>(migratedSinkId.value());
      if (settings()->value(QStringLiteral(SETTINGS_DEVICE), SETTINGS_DEFAULT_DEVICE) != migratedValue) {
        settings()->setValue(QStringLiteral(SETTINGS_DEVICE), migratedValue);
      }
      settings()->setValue(QStringLiteral(SETTINGS_DEVICE_ID_MIGRATION_DONE), true);
    }
    else if (!migrationDone) {
      settings()->setValue(QStringLiteral(SETTINGS_DEVICE_ID_MIGRATION_DONE), true);
    }

    const QVariant resolvedSinkSetting = settings()->value(QStringLiteral(SETTINGS_DEVICE), SETTINGS_DEFAULT_DEVICE);
    m_defaultSinkId = chooseSinkId(sinkIds, resolvedSinkSetting, observedDefaultSinkId());

    AudioDevice* newDefaultSink = findSinkById(sinks, m_defaultSinkId);
    if (!newDefaultSink) {
      newDefaultSink = sinks.first();
      m_defaultSinkId = newDefaultSink->index();
    }

    setDefaultSink(newDefaultSink);

    m_engine->setIgnoreMaxVolume(
        settings()->value(QStringLiteral(SETTINGS_IGNORE_MAX_VOLUME), SETTINGS_DEFAULT_IGNORE_MAX_VOLUME).toBool());
  }
  else {
    setDefaultSink(nullptr);
  }

  const auto& sources = m_engine->sources();
  if (!sources.isEmpty()) {
    QList<uint> sourceIds;
    sourceIds.reserve(sources.size());
    for (const AudioDevice* source : sources) {
      sourceIds.append(source->index());
    }

    m_defaultSourceId = chooseSinkId(sourceIds, QVariant(SETTINGS_DEFAULT_DEVICE), observedDefaultSourceId());
    AudioDevice* defaultSource = findSourceById(sources, m_defaultSourceId);
    if (!defaultSource) {
      defaultSource = sources.first();
      m_defaultSourceId = defaultSource->index();
    }

    setDefaultSource(defaultSource);
  }
  else {
    setDefaultSource(nullptr);
  }

  if (m_configDialog) {
    m_configDialog->setSinkList(sinks);
  }
}

void OneG4Volume::handleEngineStateChanged() {
  if (QThread::currentThread() != thread()) {
    QPointer<OneG4Volume> self(this);
    QMetaObject::invokeMethod(
        this,
        [self]() {
          if (!self) {
            return;
          }
          self->handleEngineStateChanged();
        },
        Qt::QueuedConnection);
    return;
  }

  if (!m_volumeButton || !m_volumeButton->volumePopup()) {
    return;
  }

  if (!m_engine) {
    m_volumeButton->volumePopup()->setBackendAvailable(false, tr("No audio engine is available"));
    return;
  }

  const AudioEngine::BackendHealthSnapshot health = m_engine->backendHealth();
  const bool backendAvailable = health.state == AudioEngine::BackendHealthState::Ready ||
                                health.state == AudioEngine::BackendHealthState::Unknown;

  m_volumeButton->volumePopup()->setBackendAvailable(backendAvailable, health.message);

  const auto& sinks = m_engine->sinks();
  if (!sinks.isEmpty()) {
    QList<uint> sinkIds;
    sinkIds.reserve(sinks.size());
    for (const AudioDevice* sink : sinks) {
      sinkIds.append(sink->index());
    }

    const QVariant storedSinkSetting = settings()->value(QStringLiteral(SETTINGS_DEVICE), SETTINGS_DEFAULT_DEVICE);
    const uint desiredSinkId = chooseSinkId(sinkIds, storedSinkSetting, observedDefaultSinkId());
    if (desiredSinkId != m_defaultSinkId) {
      AudioDevice* desiredSink = findSinkById(sinks, desiredSinkId);
      if (desiredSink) {
        m_defaultSinkId = desiredSinkId;
        setDefaultSink(desiredSink);
      }
    }
  }
  else {
    setDefaultSink(nullptr);
  }

  const auto& sources = m_engine->sources();
  if (!sources.isEmpty()) {
    QList<uint> sourceIds;
    sourceIds.reserve(sources.size());
    for (const AudioDevice* source : sources) {
      sourceIds.append(source->index());
    }

    const uint desiredSourceId = chooseSinkId(sourceIds, QVariant(SETTINGS_DEFAULT_DEVICE), observedDefaultSourceId());
    if (desiredSourceId != m_defaultSourceId) {
      AudioDevice* desiredSource = findSourceById(sources, desiredSourceId);
      if (desiredSource) {
        m_defaultSourceId = desiredSourceId;
        setDefaultSource(desiredSource);
      }
    }
  }
  else {
    setDefaultSource(nullptr);
  }

  if (m_configDialog) {
    m_configDialog->setSinkList(sinks);
  }
}

void OneG4Volume::setDefaultSink(AudioDevice* sink) {
  if (sink == m_defaultSink) {
    return;
  }

  if (m_defaultSink) {
    disconnect(m_defaultSink, nullptr, this, nullptr);
  }

  m_defaultSink = sink;
  m_defaultSinkId = m_defaultSink ? m_defaultSink->index() : 0U;

  if (m_volumeButton && m_volumeButton->volumePopup()) {
    m_volumeButton->volumePopup()->setDevice(m_defaultSink);
  }

  if (auto* sink = m_defaultSink.data()) {
    connect(sink, &AudioDevice::volumeChanged, this, [this] { showNotification(); }, Qt::QueuedConnection);
    connect(sink, &AudioDevice::muteChanged, this, [this] { showNotification(); }, Qt::QueuedConnection);
  }
}

void OneG4Volume::setDefaultSource(AudioDevice* source) {
  if (source == m_defaultSource) {
    return;
  }

  m_defaultSource = source;
  m_defaultSourceId = m_defaultSource ? m_defaultSource->index() : 0U;

  if (m_volumeButton && m_volumeButton->volumePopup()) {
    m_volumeButton->volumePopup()->setInputDevice(m_defaultSource);
  }
}

std::optional<uint> OneG4Volume::observedDefaultSinkId() const {
  if (!m_engine) {
    return std::nullopt;
  }

  const QList<AudioEngine::SinkSnapshot> sinkSnapshots = m_engine->sinkSnapshots();
  for (const AudioEngine::SinkSnapshot& sinkSnapshot : sinkSnapshots) {
    if (sinkSnapshot.isDefault) {
      return sinkSnapshot.runtimeId;
    }
  }

  return std::nullopt;
}

std::optional<uint> OneG4Volume::observedDefaultSourceId() const {
  if (!m_engine) {
    return std::nullopt;
  }

  const AudioEngine::StateSnapshot state = m_engine->stateSnapshot();
  for (const AudioEngine::LogicalEndpointSnapshot& endpoint : state.logicalEndpoints) {
    if (endpoint.direction == AudioEngine::EndpointDirection::Input && endpoint.isDefault) {
      return endpoint.runtimeId;
    }
  }

  return std::nullopt;
}

AudioDevice* OneG4Volume::findSinkById(const QList<AudioDevice*>& sinks, uint sinkId) const {
  for (AudioDevice* sink : sinks) {
    if (sink && sink->index() == sinkId) {
      return sink;
    }
  }
  return nullptr;
}

AudioDevice* OneG4Volume::findSourceById(const QList<AudioDevice*>& sources, uint sourceId) const {
  for (AudioDevice* source : sources) {
    if (source && source->index() == sourceId) {
      return source;
    }
  }
  return nullptr;
}

QWidget* OneG4Volume::widget() {
  return m_volumeButton;
}

void OneG4Volume::realign() {}

QDialog* OneG4Volume::configureDialog() {
  if (!m_configDialog) {
    m_configDialog = new OneG4VolumeConfiguration(settings(), false);
    m_configDialog->setAttribute(Qt::WA_DeleteOnClose, true);

    if (m_engine) {
      m_configDialog->setSinkList(m_engine->sinks());
    }

    connect(m_configDialog, &QObject::destroyed, this, [this] { m_configDialog = nullptr; });
  }

  return m_configDialog;
}

void OneG4Volume::openExternalMixer() {
  if (m_volumeButton) {
    m_volumeButton->hideVolumeSlider();
  }

  if (!m_engine) {
    qCWarning(lcVolumeUi) << "OneG4Volume: mixer requested but no audio engine is available";
    QMessageBox::warning(m_volumeButton, tr("Audio"), tr("No audio engine is available"));
    return;
  }

  if (!m_mixerDialog) {
    m_mixerDialog = create_1g4_mixer_dialog();
    if (!m_mixerDialog) {
      qCWarning(lcVolumeUi) << "OneG4Volume: failed to create mixer dialog";
      QMessageBox::warning(m_volumeButton, tr("Audio"), tr("Failed to create mixer dialog"));
      return;
    }
    // Don't parent to volume button to avoid focus issues with panel window.
    m_mixerDialog->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(m_mixerDialog, &QObject::destroyed, this, [this] { m_mixerDialog = nullptr; });
  }

  m_mixerDialog->show();
  m_mixerDialog->raise();
}

void OneG4Volume::showNotification() const {
  if (m_alwaysShowNotifications) {
    if (Q_LIKELY(m_defaultSink)) {
      const int vol = m_defaultSink->volume();
      const bool muted = m_defaultSink->mute();

      m_notification->setSummary(tr("Volume: %1%2").arg(QString::number(vol), muted ? tr(" (muted)") : QString()));

      m_notification->update();
    }
  }
}
