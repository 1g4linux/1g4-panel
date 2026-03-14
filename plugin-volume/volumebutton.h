/* plugin-volume/volumebutton.h
 * Volume control plugin implementation
 */

#ifndef VOLUMEBUTTON_H
#define VOLUMEBUTTON_H

#include <QToolButton>
#include <QTimer>

class VolumePopup;
class IOneG4PanelPlugin;

class VolumeButton : public QToolButton {
  Q_OBJECT
 public:
  VolumeButton(IOneG4PanelPlugin* plugin, QWidget* parent = nullptr);
  ~VolumeButton();

  void setMuteOnMiddleClick(bool state);

  VolumePopup* volumePopup() const { return m_volumePopup; }

 public slots:
  void hideVolumeSlider();
  void showVolumeSlider();

 signals:
  void deviceChanged();

 protected:
  void wheelEvent(QWheelEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 private slots:
  void toggleVolumeSlider();
  void handleStockIconChanged(const QString& iconName);

 private:
  VolumePopup* m_volumePopup;
  IOneG4PanelPlugin* mPlugin;
  QTimer m_popupHideTimer;
  bool m_muteOnMiddleClick;
};

#endif  // VOLUMEBUTTON_H
