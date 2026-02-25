#include "audiodevice.h"
#include "audioengine.h"
#include "volumepopup.h"

#include <QMetaObject>
#include <QPushButton>
#include <QSlider>
#include <QThread>
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
  void userVolumeSliderChangeUpdatesOutputDeviceVolume();
  void userMuteToggleUpdatesOutputDeviceMuteState();
  void backendVolumeUpdatesDoNotMoveSliderWhileDragging();
  void staleDeferredBackendVolumeIsDiscardedAfterFurtherDrag();
  void backendUnavailableShowsErrorIconAndDisablesControls();
  void noDeviceStateSurfacesExplicitStatus();
  void backendRecoveryRestoresInteractiveVolumeState();
  void offUiThreadVolumeUpdateIsRescheduledToUiThread();
};

void VolumePopupDragTest::userVolumeSliderChangeUpdatesOutputDeviceVolume() {
  VolumePopupDummyEngine engine;
  AudioDevice* sink = engine.addSink(QStringLiteral("alsa_output.popup-user-volume"), 25);
  QVERIFY(sink != nullptr);

  VolumePopup popup;
  popup.setDevice(sink);

  QSlider* slider = popup.volumeSlider();
  QVERIFY(slider != nullptr);
  QCOMPARE(slider->value(), 25);

  slider->setValue(72);

  QCOMPARE(sink->volume(), 72);
  QCOMPARE(slider->toolTip(), QStringLiteral("72%"));
}

void VolumePopupDragTest::userMuteToggleUpdatesOutputDeviceMuteState() {
  VolumePopupDummyEngine engine;
  AudioDevice* sink = engine.addSink(QStringLiteral("alsa_output.popup-user-mute"), 40);
  QVERIFY(sink != nullptr);

  VolumePopup popup;
  popup.setDevice(sink);

  QPushButton* muteButton = nullptr;
  for (QPushButton* candidate : popup.findChildren<QPushButton*>()) {
    if (candidate && candidate->isCheckable()) {
      muteButton = candidate;
      break;
    }
  }

  QVERIFY(muteButton != nullptr);
  QCOMPARE(muteButton->isChecked(), false);
  QCOMPARE(sink->mute(), false);

  QTest::mouseClick(muteButton, Qt::LeftButton);
  QCOMPARE(muteButton->isChecked(), true);
  QCOMPARE(sink->mute(), true);

  QTest::mouseClick(muteButton, Qt::LeftButton);
  QCOMPARE(muteButton->isChecked(), false);
  QCOMPARE(sink->mute(), false);
}

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
  QCoreApplication::processEvents();
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
  QCoreApplication::processEvents();
  QCOMPARE(slider->value(), 60);

  slider->setValue(80);
  slider->setSliderDown(false);
  QCOMPARE(slider->value(), 80);
}

void VolumePopupDragTest::backendUnavailableShowsErrorIconAndDisablesControls() {
  VolumePopupDummyEngine engine;
  AudioDevice* sink = engine.addSink(QStringLiteral("alsa_output.popup-backend"), 35);
  QVERIFY(sink != nullptr);

  VolumePopup popup;
  popup.setDevice(sink);
  QSlider* slider = popup.volumeSlider();
  QVERIFY(slider != nullptr);

  QSignalSpy stockIconSpy(&popup, &VolumePopup::stockIconChanged);
  popup.setBackendAvailable(false, QStringLiteral("PipeWire backend unavailable"));

  QCOMPARE(slider->isEnabled(), false);
  QCOMPARE(slider->toolTip(), QStringLiteral("Audio backend unavailable: PipeWire backend unavailable"));
  QVERIFY(!stockIconSpy.isEmpty());
  QCOMPARE(stockIconSpy.last().at(0).toString(), QStringLiteral("dialog-error-panel"));
}

void VolumePopupDragTest::noDeviceStateSurfacesExplicitStatus() {
  VolumePopup popup;
  QSlider* slider = popup.volumeSlider();
  QVERIFY(slider != nullptr);

  QSignalSpy stockIconSpy(&popup, &VolumePopup::stockIconChanged);

  popup.setBackendAvailable(false, QStringLiteral("temporary outage"));
  popup.setBackendAvailable(true, QString());

  QCOMPARE(slider->isEnabled(), false);
  QCOMPARE(slider->toolTip(), QStringLiteral("No audio output device"));
  QVERIFY(!stockIconSpy.isEmpty());
  QCOMPARE(stockIconSpy.last().at(0).toString(), QStringLiteral("audio-volume-muted-panel"));
}

void VolumePopupDragTest::backendRecoveryRestoresInteractiveVolumeState() {
  VolumePopupDummyEngine engine;
  AudioDevice* sink = engine.addSink(QStringLiteral("alsa_output.popup-recovery"), 20);
  QVERIFY(sink != nullptr);

  VolumePopup popup;
  popup.setDevice(sink);
  QSlider* slider = popup.volumeSlider();
  QVERIFY(slider != nullptr);

  QSignalSpy stockIconSpy(&popup, &VolumePopup::stockIconChanged);
  popup.setBackendAvailable(false, QStringLiteral("PipeWire stopped"));
  popup.setBackendAvailable(true, QString());

  QCOMPARE(slider->isEnabled(), true);
  QCOMPARE(slider->toolTip(), QStringLiteral("20%"));
  QVERIFY(!stockIconSpy.isEmpty());
  QCOMPARE(stockIconSpy.last().at(0).toString(), QStringLiteral("audio-volume-low-panel"));
}

class WorkerVolumeUpdateInvoker : public QObject {
  Q_OBJECT

 public:
  WorkerVolumeUpdateInvoker(VolumePopup* popup, int volume) : m_popup(popup), m_volume(volume) {}

 public slots:
  void invokeDirectUpdate() {
    QMetaObject::invokeMethod(m_popup, "handleDeviceVolumeChanged", Qt::DirectConnection, Q_ARG(int, m_volume));
  }

 private:
  VolumePopup* m_popup;
  int m_volume;
};

void VolumePopupDragTest::offUiThreadVolumeUpdateIsRescheduledToUiThread() {
  VolumePopupDummyEngine engine;
  AudioDevice* sink = engine.addSink(QStringLiteral("alsa_output.popup-thread"), 20);
  QVERIFY(sink != nullptr);

  VolumePopup popup;
  popup.setDevice(sink);
  QSlider* slider = popup.volumeSlider();
  QVERIFY(slider != nullptr);
  QCOMPARE(slider->value(), 20);

  QThread workerThread;
  WorkerVolumeUpdateInvoker invoker(&popup, 65);
  invoker.moveToThread(&workerThread);
  workerThread.start();

  QVERIFY(QMetaObject::invokeMethod(&invoker, "invokeDirectUpdate", Qt::BlockingQueuedConnection));

  // The worker-thread call should be marshaled back to popup's thread.
  QCOMPARE(slider->value(), 20);
  QTRY_COMPARE_WITH_TIMEOUT(slider->value(), 65, 200);

  workerThread.quit();
  QVERIFY(workerThread.wait(1000));
}

QTEST_MAIN(VolumePopupDragTest)

#include "volumepopup_drag_test.moc"
