#define private public
#include "oneg4volume.h"
#undef private

#include "audiodevice.h"
#include "audioengine.h"
#include "oneg4volumeconfiguration.h"
#include "pluginsettings_p.h"
#include "volumebutton.h"
#include "volumepopup.h"

#include <OneG4/Settings.h>

#include <QTemporaryDir>
#include <QtTest/QtTest>

QDialog* create_1g4_mixer_dialog() {
  return nullptr;
}

class VolumeDefaultSyncDummyPanel final : public IOneG4Panel {
 public:
  Position position() const override {
    return PositionBottom;
  }

  int iconSize() const override {
    return 24;
  }

  int lineCount() const override {
    return 1;
  }

  QRect globalGeometry() const override {
    return QRect(0, 0, 1920, 32);
  }

  QRect calculatePopupWindowPos(const QPoint& absolutePos, const QSize& windowSize) const override {
    return QRect(absolutePos, windowSize);
  }

  QRect calculatePopupWindowPos(const IOneG4PanelPlugin* /*plugin*/, const QSize& windowSize) const override {
    return QRect(QPoint(0, 0), windowSize);
  }

  void willShowWindow(QWidget* /*w*/) override {}

  void pluginFlagsChanged(const IOneG4PanelPlugin* /*plugin*/) override {}

  bool isLocked() const override {
    return false;
  }
};

class VolumeDefaultSyncDummyEngine final : public AudioEngine {
  Q_OBJECT

 public:
  explicit VolumeDefaultSyncDummyEngine(QObject* parent = nullptr) : AudioEngine(parent) {}

  int volumeMax(AudioDevice* /*device*/) const override {
    return 100;
  }

  const QString backendName() const override {
    return QStringLiteral("volume-default-sync-dummy");
  }

  AudioDevice* addSink(const QString& name, const QString& description, uint runtimeId) {
    auto* sink = new AudioDevice(Sink, this, this);
    sink->setName(name);
    sink->setDescription(description);
    sink->setIndex(runtimeId);
    sink->setVolumeNoCommit(55);
    sink->setMuteNoCommit(false);
    sink->setCardId(1);
    m_sinks.append(sink);
    emit sinkListChanged();
    return sink;
  }

  AudioDevice* addSource(const QString& name, const QString& description, uint runtimeId) {
    auto* source = new AudioDevice(Source, this, this);
    source->setName(name);
    source->setDescription(description);
    source->setIndex(runtimeId);
    source->setVolumeNoCommit(45);
    source->setMuteNoCommit(false);
    source->setCardId(1);
    m_sources.append(source);
    emit sinkListChanged();
    return source;
  }

  void setObservedDefaultSinkByRuntimeId(uint runtimeId) {
    AudioDevice* sink = nullptr;
    for (AudioDevice* candidate : std::as_const(m_sinks)) {
      if (candidate && candidate->index() == runtimeId) {
        sink = candidate;
        break;
      }
    }

    setObservedDefaultEndpoint(EndpointDirection::Output, sink);
  }

  void setObservedDefaultSourceByRuntimeId(uint runtimeId) {
    AudioDevice* source = nullptr;
    for (AudioDevice* candidate : std::as_const(m_sources)) {
      if (candidate && candidate->index() == runtimeId) {
        source = candidate;
        break;
      }
    }

    setObservedDefaultEndpoint(EndpointDirection::Input, source);
  }

 protected:
  bool commitDeviceVolume(AudioDevice* device) override {
    Q_UNUSED(device);
    return true;
  }

  bool setMute(AudioDevice* device, bool state) override {
    Q_UNUSED(device);
    Q_UNUSED(state);
    return true;
  }
};

class VolumeDefaultSinkSyncTest : public QObject {
  Q_OBJECT

 private slots:
  void autoSelectionTracksObservedDefaultSink();
  void explicitSelectionOverridesObservedDefaultSink();
  void observedDefaultInputTracksPopupInputDevice();
};

void VolumeDefaultSinkSyncTest::autoSelectionTracksObservedDefaultSink() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  const QString settingsPath = tempDir.filePath(QStringLiteral("panel-test.ini"));
  OneG4::Settings settings(settingsPath, QSettings::IniFormat);
  std::unique_ptr<PluginSettings> pluginSettings(
      PluginSettingsFactory::create(&settings, QStringLiteral("volume-test"), &settings));
  QVERIFY(pluginSettings != nullptr);

  pluginSettings->setValue(QStringLiteral(SETTINGS_DEVICE), SETTINGS_DEFAULT_DEVICE);
  pluginSettings->setValue(QStringLiteral(SETTINGS_AUDIO_ENGINE), QStringLiteral("UnknownBackend"));

  VolumeDefaultSyncDummyPanel panel;
  const IOneG4PanelPluginStartupInfo startupInfo{&panel, pluginSettings.get(), nullptr};
  OneG4Volume plugin(startupInfo);

  auto* engine = new VolumeDefaultSyncDummyEngine;
  engine->addSink(QStringLiteral("alsa_output.primary"), QStringLiteral("Primary Output"), 10U);
  engine->addSink(QStringLiteral("alsa_output.secondary"), QStringLiteral("Secondary Output"), 20U);
  engine->setObservedDefaultSinkByRuntimeId(10U);

  plugin.setAudioEngine(engine);

  QVERIFY(plugin.m_defaultSink != nullptr);
  QCOMPARE(plugin.m_defaultSink->index(), 10U);
  QVERIFY(plugin.m_volumeButton != nullptr);
  QVERIFY(plugin.m_volumeButton->volumePopup()->device() != nullptr);
  QCOMPARE(plugin.m_volumeButton->volumePopup()->device()->index(), 10U);

  engine->setObservedDefaultSinkByRuntimeId(20U);

  QTRY_VERIFY_WITH_TIMEOUT(plugin.m_defaultSink != nullptr, 300);
  QTRY_COMPARE_WITH_TIMEOUT(plugin.m_defaultSink->index(), 20U, 300);
  QVERIFY(plugin.m_volumeButton->volumePopup()->device() != nullptr);
  QCOMPARE(plugin.m_volumeButton->volumePopup()->device()->index(), 20U);
}

void VolumeDefaultSinkSyncTest::explicitSelectionOverridesObservedDefaultSink() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  const QString settingsPath = tempDir.filePath(QStringLiteral("panel-test.ini"));
  OneG4::Settings settings(settingsPath, QSettings::IniFormat);
  std::unique_ptr<PluginSettings> pluginSettings(
      PluginSettingsFactory::create(&settings, QStringLiteral("volume-test"), &settings));
  QVERIFY(pluginSettings != nullptr);

  pluginSettings->setValue(QStringLiteral(SETTINGS_DEVICE), 10);
  pluginSettings->setValue(QStringLiteral(SETTINGS_AUDIO_ENGINE), QStringLiteral("UnknownBackend"));

  VolumeDefaultSyncDummyPanel panel;
  const IOneG4PanelPluginStartupInfo startupInfo{&panel, pluginSettings.get(), nullptr};
  OneG4Volume plugin(startupInfo);

  auto* engine = new VolumeDefaultSyncDummyEngine;
  engine->addSink(QStringLiteral("alsa_output.primary"), QStringLiteral("Primary Output"), 10U);
  engine->addSink(QStringLiteral("alsa_output.secondary"), QStringLiteral("Secondary Output"), 20U);
  engine->setObservedDefaultSinkByRuntimeId(20U);

  plugin.setAudioEngine(engine);

  QVERIFY(plugin.m_defaultSink != nullptr);
  QCOMPARE(plugin.m_defaultSink->index(), 10U);

  engine->setObservedDefaultSinkByRuntimeId(20U);

  QTest::qWait(50);
  QVERIFY(plugin.m_defaultSink != nullptr);
  QCOMPARE(plugin.m_defaultSink->index(), 10U);
}

void VolumeDefaultSinkSyncTest::observedDefaultInputTracksPopupInputDevice() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  const QString settingsPath = tempDir.filePath(QStringLiteral("panel-test.ini"));
  OneG4::Settings settings(settingsPath, QSettings::IniFormat);
  std::unique_ptr<PluginSettings> pluginSettings(
      PluginSettingsFactory::create(&settings, QStringLiteral("volume-test"), &settings));
  QVERIFY(pluginSettings != nullptr);

  pluginSettings->setValue(QStringLiteral(SETTINGS_DEVICE), SETTINGS_DEFAULT_DEVICE);
  pluginSettings->setValue(QStringLiteral(SETTINGS_AUDIO_ENGINE), QStringLiteral("UnknownBackend"));

  VolumeDefaultSyncDummyPanel panel;
  const IOneG4PanelPluginStartupInfo startupInfo{&panel, pluginSettings.get(), nullptr};
  OneG4Volume plugin(startupInfo);

  auto* engine = new VolumeDefaultSyncDummyEngine;
  engine->addSink(QStringLiteral("alsa_output.primary"), QStringLiteral("Primary Output"), 10U);
  engine->addSource(QStringLiteral("alsa_input.primary"), QStringLiteral("Primary Input"), 101U);
  engine->addSource(QStringLiteral("alsa_input.secondary"), QStringLiteral("Secondary Input"), 202U);
  engine->setObservedDefaultSinkByRuntimeId(10U);
  engine->setObservedDefaultSourceByRuntimeId(101U);

  plugin.setAudioEngine(engine);

  QVERIFY(plugin.m_defaultSource != nullptr);
  QCOMPARE(plugin.m_defaultSource->index(), 101U);
  QVERIFY(plugin.m_volumeButton != nullptr);
  QVERIFY(plugin.m_volumeButton->volumePopup()->inputDevice() != nullptr);
  QCOMPARE(plugin.m_volumeButton->volumePopup()->inputDevice()->index(), 101U);

  engine->setObservedDefaultSourceByRuntimeId(202U);

  QTRY_VERIFY_WITH_TIMEOUT(plugin.m_defaultSource != nullptr, 300);
  QTRY_COMPARE_WITH_TIMEOUT(plugin.m_defaultSource->index(), 202U, 300);
  QVERIFY(plugin.m_volumeButton->volumePopup()->inputDevice() != nullptr);
  QCOMPARE(plugin.m_volumeButton->volumePopup()->inputDevice()->index(), 202U);
}

QTEST_MAIN(VolumeDefaultSinkSyncTest)

#include "volume_default_sink_sync_test.moc"
