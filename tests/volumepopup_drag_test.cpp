#include "audiodevice.h"
#include "audioengine.h"
#include "volumepopup.h"

#include <QSlider>
#include <QtTest/QtTest>

class VolumePopupDummyEngine : public AudioEngine {
  Q_OBJECT

 public:
  explicit VolumePopupDummyEngine(QObject* parent = nullptr) : AudioEngine(parent) {}

  int volumeMax(AudioDevice* /*device*/) const override {
    return 100;
  }

  const QString backendName() const override {
    return QStringLiteral("volumepopup-dummy");
  }

  AudioDevice* addSink(const QString& name, int initialVolume) {
    auto* sink = new AudioDevice(Sink, this, this);
    sink->setName(name);
    sink->setDescription(name);
    sink->setIndex(static_cast<uint>(m_sinks.size() + 1));
    sink->setVolumeNoCommit(initialVolume);
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

class VolumePopupDragTest : public QObject {
  Q_OBJECT

 private slots:
  void backendVolumeUpdatesDoNotMoveSliderWhileDragging();
  void staleDeferredBackendVolumeIsDiscardedAfterFurtherDrag();
};

void VolumePopupDragTest::backendVolumeUpdatesDoNotMoveSliderWhileDragging() {
  VolumePopupDummyEngine engine;
  AudioDevice* sink = engine.addSink(QStringLiteral("alsa_output.popup-drag"), 20);
  QVERIFY(sink != nullptr);

  VolumePopup popup;
  popup.setDevice(sink);

  QSlider* slider = popup.volumeSlider();
  QVERIFY(slider != nullptr);
  QCOMPARE(slider->value(), 20);

  slider->setSliderDown(true);
  slider->setValue(70);
  QCOMPARE(slider->value(), 70);

  sink->setVolumeNoCommit(30);
  sink->setVolumeNoCommit(40);
  QCOMPARE(slider->value(), 70);

  slider->setSliderDown(false);
  QCOMPARE(slider->value(), 40);
  QCOMPARE(slider->toolTip(), QStringLiteral("40%"));
}

void VolumePopupDragTest::staleDeferredBackendVolumeIsDiscardedAfterFurtherDrag() {
  VolumePopupDummyEngine engine;
  AudioDevice* sink = engine.addSink(QStringLiteral("alsa_output.popup-stale"), 25);
  QVERIFY(sink != nullptr);

  VolumePopup popup;
  popup.setDevice(sink);

  QSlider* slider = popup.volumeSlider();
  QVERIFY(slider != nullptr);
  QCOMPARE(slider->value(), 25);

  slider->setSliderDown(true);
  slider->setValue(60);
  sink->setVolumeNoCommit(15);
  QCOMPARE(slider->value(), 60);

  slider->setValue(80);
  slider->setSliderDown(false);
  QCOMPARE(slider->value(), 80);
}

QTEST_MAIN(VolumePopupDragTest)

#include "volumepopup_drag_test.moc"
