#include "audioengine.h"
#include "audiodevice.h"
#include "oneg4volumeconfiguration.h"
#include "pluginsettings_p.h"

#include <OneG4/Settings.h>

#include <QComboBox>
#include <QShowEvent>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class VolumeConfigurationDummyEngine : public AudioEngine {
  Q_OBJECT

 public:
  explicit VolumeConfigurationDummyEngine(QObject* parent = nullptr) : AudioEngine(parent) {}

  int volumeMax(AudioDevice* /*device*/) const override {
    return 100;
  }

  const QString backendName() const override {
    return QStringLiteral("volume-config-dummy");
  }

  AudioDevice* addSink(const QString& name, const QString& description, uint index) {
    auto* sink = new AudioDevice(Sink, this, this);
    sink->setName(name);
    sink->setDescription(description);
    sink->setIndex(index);
    sink->setVolumeNoCommit(50);
    sink->setMuteNoCommit(false);
    sink->setCardId(1);
    m_sinks.append(sink);
    return sink;
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

class VolumeConfigurationLazyRebuildTest : public QObject {
  Q_OBJECT

 private slots:
  void sinkMenuRebuildIsDeferredUntilDialogIsVisible();
};

class TestableVolumeConfigurationDialog : public OneG4VolumeConfiguration {
 public:
  using OneG4VolumeConfiguration::OneG4VolumeConfiguration;

  void triggerShowEventForTest() {
    QShowEvent event;
    OneG4VolumeConfiguration::showEvent(&event);
  }
};

void VolumeConfigurationLazyRebuildTest::sinkMenuRebuildIsDeferredUntilDialogIsVisible() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  const QString settingsPath = tempDir.filePath(QStringLiteral("panel-test.ini"));
  OneG4::Settings settings(settingsPath, QSettings::IniFormat);
  std::unique_ptr<PluginSettings> pluginSettings(
      PluginSettingsFactory::create(&settings, QStringLiteral("volume-test"), &settings));
  QVERIFY(pluginSettings != nullptr);

  VolumeConfigurationDummyEngine engine;
  AudioDevice* sinkA =
      engine.addSink(QStringLiteral("alsa_output.cardA"), QStringLiteral("Built-in Analog"), static_cast<uint>(10));
  AudioDevice* sinkB =
      engine.addSink(QStringLiteral("alsa_output.cardB"), QStringLiteral("Built-in HDMI"), static_cast<uint>(20));
  QVERIFY(sinkA != nullptr);
  QVERIFY(sinkB != nullptr);

  TestableVolumeConfigurationDialog dialog(pluginSettings.get(), false);
  QComboBox* combo = dialog.findChild<QComboBox*>(QStringLiteral("devAddedCombo"));
  QVERIFY(combo != nullptr);
  QCOMPARE(combo->count(), 0);

  dialog.setSinkList({sinkA, sinkB});

  // Hidden dialog should not rebuild sink selection UI immediately.
  QCOMPARE(dialog.isVisible(), false);
  QCOMPARE(combo->count(), 0);

  dialog.triggerShowEventForTest();
  QCOMPARE(combo->count(), 2);

  AudioDevice* sinkC =
      engine.addSink(QStringLiteral("alsa_output.cardC"), QStringLiteral("USB Audio"), static_cast<uint>(30));
  QVERIFY(sinkC != nullptr);

  dialog.setSinkList({sinkA, sinkB, sinkC});
  QCOMPARE(combo->count(), 2);

  dialog.triggerShowEventForTest();
  QCOMPARE(combo->count(), 3);
}

QTEST_MAIN(VolumeConfigurationLazyRebuildTest)

#include "volumeconfiguration_lazy_rebuild_test.moc"
