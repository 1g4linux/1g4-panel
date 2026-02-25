/* plugin-volume/volumepopup.h
 * Volume control plugin implementation
 */

#ifndef VOLUMEPOPUP_H
#define VOLUMEPOPUP_H

#include <QDialog>
#include <QPointer>

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

  AudioDevice* device() const { return m_device; }
  void setDevice(AudioDevice* device);
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
  void handleMuteToggleClicked();
  void handleExternalMixerClicked();
  void handleDeviceVolumeChanged(int volume);
  void handleDeviceMuteChanged(bool mute);

 private:
  void realign();
  void updateStockIcon();

  QSlider* m_volumeSlider;
  QPushButton* m_muteToggleButton;
  QPushButton* m_externalMixerButton;
  QPoint m_pos;
  Qt::Corner m_anchor;
  QPointer<AudioDevice> m_device;
};

#endif  // VOLUMEPOPUP_H
