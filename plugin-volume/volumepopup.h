/* plugin-volume/volumepopup.h
 * Volume control plugin implementation
 */

#ifndef VOLUMEPOPUP_H
#define VOLUMEPOPUP_H

#include <QDialog>
#include <QPointer>
#include <QString>

class QSlider;
class QPushButton;
class AudioDevice;

class VolumePopup : public QDialog {
  Q_OBJECT
 public:
  VolumePopup(QWidget* parent = nullptr);

  void openAt(QPoint pos, Qt::Corner anchor);
  void handleWheelEvent(QWheelEvent* event);

  QSlider* volumeSlider() const { return m_volumeSlider; }
  QSlider* inputVolumeSlider() const { return m_inputVolumeSlider; }

  AudioDevice* device() const { return m_device; }
  AudioDevice* inputDevice() const { return m_inputDevice; }
  void setDevice(AudioDevice* device);
  void setInputDevice(AudioDevice* device);
  void setBackendAvailable(bool available, const QString& statusMessage = QString());
  void setSliderStep(int step);

 signals:
  void mouseEntered();
  void mouseLeft();
  void externalMixerRequested();

  // void volumeChanged(int value);
  void deviceChanged();
  void stockIconChanged(const QString& iconName);

 protected:
  void resizeEvent(QResizeEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;
  bool event(QEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private slots:
  void handleSliderValueChanged(int value);
  void handleSliderPressed();
  void handleSliderReleased();
  void handleMuteToggleClicked();
  void handleExternalMixerClicked();
  void handleDeviceVolumeChanged(int volume);
  void handleDeviceMuteChanged(bool mute);
  void handleInputSliderValueChanged(int value);
  void handleInputSliderPressed();
  void handleInputSliderReleased();
  void handleInputMuteToggleClicked();
  void handleInputDeviceVolumeChanged(int volume);
  void handleInputDeviceMuteChanged(bool mute);

 private:
  void realign();
  void updateStockIcon();
  void applyVolumeToSlider(int volume);
  void applyInputVolumeToSlider(int volume);
  void updateControlAvailability();
  void updateStatusToolTip();

  QSlider* m_volumeSlider;
  QSlider* m_inputVolumeSlider;
  QPushButton* m_muteToggleButton;
  QPushButton* m_inputMuteToggleButton;
  QPushButton* m_externalMixerButton;
  QPoint m_pos;
  Qt::Corner m_anchor;
  QPointer<AudioDevice> m_device;
  QPointer<AudioDevice> m_inputDevice;
  bool m_sliderDragActive;
  bool m_hasDeferredBackendVolume;
  int m_deferredBackendVolumePercent;
  bool m_inputSliderDragActive;
  bool m_hasDeferredBackendInputVolume;
  int m_deferredBackendInputVolumePercent;
  bool m_backendAvailable;
  QString m_backendStatusMessage;
};

#endif  // VOLUMEPOPUP_H
