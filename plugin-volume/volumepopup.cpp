/* plugin-volume/volumepopup.cpp
 * Volume control plugin implementation
 */

#include "volumepopup.h"

#include "audiodevice.h"

#include <OneG4/XdgIcon.h>

#include <QCursor>
#include <QDialog>
#include <QEnterEvent>
#include <QGuiApplication>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QSlider>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtGlobal>

VolumePopup::VolumePopup(QWidget* parent)
    : QDialog(
          parent,
          Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint | Qt::Popup | Qt::X11BypassWindowManagerHint),
      m_pos(0, 0),
      m_anchor(Qt::TopLeftCorner),
      m_device(nullptr) {
  m_volumeSlider = new QSlider(Qt::Vertical, this);
  m_volumeSlider->setTickPosition(QSlider::TicksBothSides);
  m_volumeSlider->setTickInterval(10);
  m_volumeSlider->setRange(0, 100);
  m_volumeSlider->installEventFilter(this);

  m_muteToggleButton = new QPushButton(this);
  m_muteToggleButton->setIcon(XdgIcon::fromTheme(QLatin1String("audio-volume-muted-panel")));
  m_muteToggleButton->setCheckable(true);
  m_muteToggleButton->setAutoDefault(false);

  m_externalMixerButton = new QPushButton(tr("Mixer"), this);
  m_externalMixerButton->setAutoDefault(false);

  auto* layout = new QVBoxLayout(this);
  layout->setSpacing(0);
  layout->setContentsMargins(QMargins());

  layout->addWidget(m_volumeSlider, 0, Qt::AlignHCenter);
  layout->addWidget(m_muteToggleButton, 0, Qt::AlignHCenter);
  layout->addWidget(m_externalMixerButton, 0, Qt::AlignHCenter);

  connect(m_volumeSlider, &QSlider::valueChanged, this, &VolumePopup::handleSliderValueChanged);
  connect(m_muteToggleButton, &QPushButton::clicked, this, &VolumePopup::handleMuteToggleClicked);
  connect(m_externalMixerButton, &QPushButton::clicked, this, &VolumePopup::handleExternalMixerClicked);
}

bool VolumePopup::event(QEvent* event) {
  if (event->type() == QEvent::WindowDeactivate) {
    hide();
  }
  return QDialog::event(event);
}

bool VolumePopup::eventFilter(QObject* watched, QEvent* event) {
  if (watched == m_volumeSlider && event->type() == QEvent::Wheel) {
    handleWheelEvent(static_cast<QWheelEvent*>(event));
    return true;
  }

  return QDialog::eventFilter(watched, event);
}

void VolumePopup::enterEvent(QEnterEvent* /*event*/) {
  emit mouseEntered();
}

void VolumePopup::leaveEvent(QEvent* /*event*/) {
  emit mouseLeft();
}

void VolumePopup::handleSliderValueChanged(int value) {
  if (!m_device) {
    return;
  }

  m_device->setVolume(value);
  m_volumeSlider->setToolTip(QStringLiteral("%1%").arg(value));

  QTimer::singleShot(0, this, [this] { QToolTip::showText(QCursor::pos(), m_volumeSlider->toolTip(), this); });
}

void VolumePopup::handleMuteToggleClicked() {
  if (!m_device) {
    return;
  }

  m_device->toggleMute();
}

void VolumePopup::handleExternalMixerClicked() {
  emit externalMixerRequested();
}

void VolumePopup::handleDeviceVolumeChanged(int volume) {
  m_volumeSlider->blockSignals(true);
  m_volumeSlider->setValue(volume);
  m_volumeSlider->setToolTip(QStringLiteral("%1%").arg(volume));

  if (auto* parent = parentWidget()) {
    parent->setToolTip(m_volumeSlider->toolTip());
  }

  m_volumeSlider->blockSignals(false);

  updateStockIcon();
}

void VolumePopup::handleDeviceMuteChanged(bool mute) {
  m_muteToggleButton->setChecked(mute);
  updateStockIcon();
}

void VolumePopup::updateStockIcon() {
  if (!m_device) {
    m_muteToggleButton->setIcon(XdgIcon::fromTheme(QLatin1String("audio-volume-muted-panel")));
    emit stockIconChanged(QStringLiteral("audio-volume-muted-panel"));
    return;
  }

  QString iconName;
  if (m_device->volume() <= 0 || m_device->mute()) {
    iconName = QLatin1String("audio-volume-muted");
  }
  else if (m_device->volume() <= 33) {
    iconName = QLatin1String("audio-volume-low");
  }
  else if (m_device->volume() <= 66) {
    iconName = QLatin1String("audio-volume-medium");
  }
  else {
    iconName = QLatin1String("audio-volume-high");
  }

  iconName.append(QLatin1String("-panel"));
  m_muteToggleButton->setIcon(XdgIcon::fromTheme(iconName));
  emit stockIconChanged(iconName);
}

void VolumePopup::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  realign();
}

void VolumePopup::openAt(QPoint pos, Qt::Corner anchor) {
  m_pos = pos;
  m_anchor = anchor;
  realign();
  show();
}

void VolumePopup::handleWheelEvent(QWheelEvent* event) {
  const int steps = event->angleDelta().y() / QWheelEvent::DefaultDeltasPerStep;
  if (steps == 0) {
    return;
  }

  m_volumeSlider->setSliderPosition(m_volumeSlider->sliderPosition() + steps * m_volumeSlider->singleStep());
}

void VolumePopup::setDevice(AudioDevice* device) {
  if (device == m_device) {
    return;
  }

  if (m_device) {
    disconnect(m_device, &AudioDevice::volumeChanged, this, &VolumePopup::handleDeviceVolumeChanged);
    disconnect(m_device, &AudioDevice::muteChanged, this, &VolumePopup::handleDeviceMuteChanged);
  }

  m_device = device;

  if (auto* dev = m_device.data()) {
    m_muteToggleButton->setChecked(dev->mute());
    handleDeviceVolumeChanged(dev->volume());
    connect(dev, &AudioDevice::volumeChanged, this, &VolumePopup::handleDeviceVolumeChanged);
    connect(dev, &AudioDevice::muteChanged, this, &VolumePopup::handleDeviceMuteChanged);
  }
  else {
    m_volumeSlider->blockSignals(true);
    m_volumeSlider->setValue(0);
    m_volumeSlider->setToolTip(QStringLiteral("0%"));
    m_volumeSlider->blockSignals(false);

    if (auto* parent = parentWidget()) {
      parent->setToolTip(m_volumeSlider->toolTip());
    }

    m_muteToggleButton->setChecked(true);
    updateStockIcon();
  }

  emit deviceChanged();
}

void VolumePopup::setSliderStep(int step) {
  m_volumeSlider->setSingleStep(step);
  m_volumeSlider->setPageStep(step * 10);
}

void VolumePopup::realign() {
  QRect rect;
  rect.setSize(sizeHint());
  switch (m_anchor) {
    case Qt::TopLeftCorner:
      rect.moveTopLeft(m_pos);
      break;

    case Qt::TopRightCorner:
      rect.moveTopRight(m_pos);
      break;

    case Qt::BottomLeftCorner:
      rect.moveBottomLeft(m_pos);
      break;

    case Qt::BottomRightCorner:
      rect.moveBottomRight(m_pos);
      break;
  }

  if (const QScreen* screen = QGuiApplication::screenAt(m_pos)) {
    const QRect geometry = screen->availableGeometry();

    if (rect.right() > geometry.right()) {
      rect.moveRight(geometry.right());
    }

    if (rect.bottom() > geometry.bottom()) {
      rect.moveBottom(geometry.bottom());
    }
  }

  move(rect.topLeft());
}
