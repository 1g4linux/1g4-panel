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
#include <QHBoxLayout>
#include <QMetaObject>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QSlider>
#include <QThread>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtGlobal>

namespace {

QString tooltipDisplayName(const AudioDevice* device, const QString& fallback) {
  if (!device) {
    return fallback;
  }

  const QString description = device->description().trimmed();
  if (!description.isEmpty()) {
    return description;
  }

  const QString name = device->name().trimmed();
  if (!name.isEmpty()) {
    return name;
  }

  return fallback;
}

QString tooltipEndpointState(const AudioDevice* device, const QString& active, const QString& inactive) {
  if (!device) {
    return inactive;
  }
  return device->enabled() ? active : inactive;
}

}  // namespace

VolumePopup::VolumePopup(QWidget* parent)
    : QDialog(
          parent,
          Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint | Qt::Popup | Qt::X11BypassWindowManagerHint),
      m_pos(0, 0),
      m_anchor(Qt::TopLeftCorner),
      m_device(nullptr),
      m_inputDevice(nullptr),
      m_sliderDragActive(false),
      m_hasDeferredBackendVolume(false),
      m_deferredBackendVolumePercent(0),
      m_inputSliderDragActive(false),
      m_hasDeferredBackendInputVolume(false),
      m_deferredBackendInputVolumePercent(0),
      m_backendAvailable(true),
      m_backendStatusMessage() {
  m_volumeSlider = new QSlider(Qt::Vertical, this);
  m_volumeSlider->setObjectName(QStringLiteral("OutputVolumeSlider"));
  m_volumeSlider->setTickPosition(QSlider::TicksBothSides);
  m_volumeSlider->setTickInterval(10);
  m_volumeSlider->setRange(0, 100);
  m_volumeSlider->installEventFilter(this);

  m_inputVolumeSlider = new QSlider(Qt::Horizontal, this);
  m_inputVolumeSlider->setObjectName(QStringLiteral("InputVolumeSlider"));
  m_inputVolumeSlider->setTickPosition(QSlider::NoTicks);
  m_inputVolumeSlider->setRange(0, 100);

  m_muteToggleButton = new QPushButton(this);
  m_muteToggleButton->setObjectName(QStringLiteral("OutputMuteToggleButton"));
  m_muteToggleButton->setIcon(XdgIcon::fromTheme(QLatin1String("audio-volume-muted-panel")));
  m_muteToggleButton->setCheckable(true);
  m_muteToggleButton->setAutoDefault(false);

  m_inputMuteToggleButton = new QPushButton(this);
  m_inputMuteToggleButton->setObjectName(QStringLiteral("InputMuteToggleButton"));
  m_inputMuteToggleButton->setIcon(XdgIcon::fromTheme(QLatin1String("audio-input-microphone-muted-panel")));
  m_inputMuteToggleButton->setCheckable(true);
  m_inputMuteToggleButton->setAutoDefault(false);

  m_externalMixerButton = new QPushButton(this);
  m_externalMixerButton->setObjectName(QStringLiteral("MixerLink"));
  m_externalMixerButton->setToolTip(tr("Launch mixer"));
  m_externalMixerButton->setIcon(XdgIcon::fromTheme(QLatin1String("audio-card")));
  m_externalMixerButton->setIconSize(QSize(16, 16));
  m_externalMixerButton->setMinimumWidth(1);
  m_externalMixerButton->setAutoDefault(false);

  auto* inputControlsLayout = new QHBoxLayout();
  inputControlsLayout->setSpacing(4);
  inputControlsLayout->setContentsMargins(QMargins());
  inputControlsLayout->addWidget(m_inputMuteToggleButton, 0, Qt::AlignVCenter);
  inputControlsLayout->addWidget(m_inputVolumeSlider, 1, Qt::AlignVCenter);

  auto* layout = new QVBoxLayout(this);
  layout->setSpacing(2);
  layout->setContentsMargins(QMargins());

  layout->addWidget(m_externalMixerButton, 0, Qt::AlignHCenter);
  layout->addWidget(m_volumeSlider, 0, Qt::AlignHCenter);
  layout->addWidget(m_muteToggleButton, 0, Qt::AlignHCenter);
  layout->addLayout(inputControlsLayout);

  connect(m_volumeSlider, &QSlider::valueChanged, this, &VolumePopup::handleSliderValueChanged);
  connect(m_volumeSlider, &QSlider::sliderPressed, this, &VolumePopup::handleSliderPressed);
  connect(m_volumeSlider, &QSlider::sliderReleased, this, &VolumePopup::handleSliderReleased);
  connect(m_muteToggleButton, &QPushButton::clicked, this, &VolumePopup::handleMuteToggleClicked);

  connect(m_inputVolumeSlider, &QSlider::valueChanged, this, &VolumePopup::handleInputSliderValueChanged);
  connect(m_inputVolumeSlider, &QSlider::sliderPressed, this, &VolumePopup::handleInputSliderPressed);
  connect(m_inputVolumeSlider, &QSlider::sliderReleased, this, &VolumePopup::handleInputSliderReleased);
  connect(m_inputMuteToggleButton, &QPushButton::clicked, this, &VolumePopup::handleInputMuteToggleClicked);

  connect(m_externalMixerButton, &QPushButton::clicked, this, &VolumePopup::handleExternalMixerClicked);

  updateControlAvailability();
  updateStatusToolTip();
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
  if (!m_backendAvailable || !m_device) {
    return;
  }

  if (m_sliderDragActive || m_volumeSlider->isSliderDown()) {
    // User motion during a drag supersedes any older deferred backend value.
    m_hasDeferredBackendVolume = false;
  }

  m_device->setVolume(value);
  updateStatusToolTip();

  QTimer::singleShot(0, this, [this] { QToolTip::showText(QCursor::pos(), m_volumeSlider->toolTip(), this); });
}

void VolumePopup::handleSliderPressed() {
  m_sliderDragActive = true;
}

void VolumePopup::handleSliderReleased() {
  m_sliderDragActive = false;
  if (!m_hasDeferredBackendVolume) {
    return;
  }

  const int deferredVolume = m_deferredBackendVolumePercent;
  m_hasDeferredBackendVolume = false;
  applyVolumeToSlider(deferredVolume);
  updateStockIcon();
}

void VolumePopup::handleMuteToggleClicked() {
  if (!m_device) {
    return;
  }

  m_device->toggleMute();
}

void VolumePopup::handleInputSliderValueChanged(int value) {
  if (!m_backendAvailable || !m_inputDevice) {
    return;
  }

  if (m_inputSliderDragActive || m_inputVolumeSlider->isSliderDown()) {
    m_hasDeferredBackendInputVolume = false;
  }

  m_inputDevice->setVolume(value);
  updateStatusToolTip();

  QTimer::singleShot(0, this, [this] { QToolTip::showText(QCursor::pos(), m_inputVolumeSlider->toolTip(), this); });
}

void VolumePopup::handleInputSliderPressed() {
  m_inputSliderDragActive = true;
}

void VolumePopup::handleInputSliderReleased() {
  m_inputSliderDragActive = false;
  if (!m_hasDeferredBackendInputVolume) {
    return;
  }

  const int deferredVolume = m_deferredBackendInputVolumePercent;
  m_hasDeferredBackendInputVolume = false;
  applyInputVolumeToSlider(deferredVolume);
}

void VolumePopup::handleInputMuteToggleClicked() {
  if (!m_inputDevice) {
    return;
  }

  m_inputDevice->toggleMute();
}

void VolumePopup::handleExternalMixerClicked() {
  emit externalMixerRequested();
}

void VolumePopup::handleDeviceVolumeChanged(int volume) {
  if (QThread::currentThread() != thread()) {
    QPointer<VolumePopup> self(this);
    QMetaObject::invokeMethod(
        this,
        [self, volume]() {
          if (!self) {
            return;
          }
          self->handleDeviceVolumeChanged(volume);
        },
        Qt::QueuedConnection);
    return;
  }

  const int boundedVolume = qBound(0, volume, 100);
  if (m_sliderDragActive || m_volumeSlider->isSliderDown()) {
    m_deferredBackendVolumePercent = boundedVolume;
    m_hasDeferredBackendVolume = true;
    updateStockIcon();
    return;
  }

  applyVolumeToSlider(boundedVolume);
  updateStockIcon();
}

void VolumePopup::handleDeviceMuteChanged(bool mute) {
  if (QThread::currentThread() != thread()) {
    QPointer<VolumePopup> self(this);
    QMetaObject::invokeMethod(
        this,
        [self, mute]() {
          if (!self) {
            return;
          }
          self->handleDeviceMuteChanged(mute);
        },
        Qt::QueuedConnection);
    return;
  }

  m_muteToggleButton->setChecked(mute);
  updateStatusToolTip();
  updateStockIcon();
}

void VolumePopup::handleInputDeviceVolumeChanged(int volume) {
  if (QThread::currentThread() != thread()) {
    QPointer<VolumePopup> self(this);
    QMetaObject::invokeMethod(
        this,
        [self, volume]() {
          if (!self) {
            return;
          }
          self->handleInputDeviceVolumeChanged(volume);
        },
        Qt::QueuedConnection);
    return;
  }

  const int boundedVolume = qBound(0, volume, 100);
  if (m_inputSliderDragActive || m_inputVolumeSlider->isSliderDown()) {
    m_deferredBackendInputVolumePercent = boundedVolume;
    m_hasDeferredBackendInputVolume = true;
    return;
  }

  applyInputVolumeToSlider(boundedVolume);
}

void VolumePopup::handleInputDeviceMuteChanged(bool mute) {
  if (QThread::currentThread() != thread()) {
    QPointer<VolumePopup> self(this);
    QMetaObject::invokeMethod(
        this,
        [self, mute]() {
          if (!self) {
            return;
          }
          self->handleInputDeviceMuteChanged(mute);
        },
        Qt::QueuedConnection);
    return;
  }

  m_inputMuteToggleButton->setChecked(mute);
  updateStatusToolTip();
}

void VolumePopup::updateStockIcon() {
  if (!m_backendAvailable) {
    m_muteToggleButton->setIcon(XdgIcon::fromTheme(QLatin1String("dialog-error-panel")));
    emit stockIconChanged(QStringLiteral("dialog-error-panel"));
    return;
  }

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
  if (!m_backendAvailable || !m_device) {
    return;
  }

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
    disconnect(m_device, &AudioDevice::nameChanged, this, &VolumePopup::updateStatusToolTip);
    disconnect(m_device, &AudioDevice::descriptionChanged, this, &VolumePopup::updateStatusToolTip);
    disconnect(m_device, &AudioDevice::enabledChanged, this, &VolumePopup::updateStatusToolTip);
  }

  m_device = device;
  m_sliderDragActive = false;
  m_hasDeferredBackendVolume = false;
  m_deferredBackendVolumePercent = 0;

  if (auto* dev = m_device.data()) {
    m_muteToggleButton->setChecked(dev->mute());
    handleDeviceVolumeChanged(dev->volume());
    connect(dev, &AudioDevice::volumeChanged, this, &VolumePopup::handleDeviceVolumeChanged, Qt::QueuedConnection);
    connect(dev, &AudioDevice::muteChanged, this, &VolumePopup::handleDeviceMuteChanged, Qt::QueuedConnection);
    connect(dev, &AudioDevice::nameChanged, this, &VolumePopup::updateStatusToolTip, Qt::QueuedConnection);
    connect(dev, &AudioDevice::descriptionChanged, this, &VolumePopup::updateStatusToolTip, Qt::QueuedConnection);
    connect(dev, &AudioDevice::enabledChanged, this, &VolumePopup::updateStatusToolTip, Qt::QueuedConnection);
  }
  else {
    m_volumeSlider->blockSignals(true);
    m_volumeSlider->setValue(0);
    m_volumeSlider->blockSignals(false);

    m_muteToggleButton->setChecked(true);
  }

  updateControlAvailability();
  updateStatusToolTip();
  updateStockIcon();
  emit deviceChanged();
}

void VolumePopup::setInputDevice(AudioDevice* device) {
  if (device == m_inputDevice) {
    return;
  }

  if (m_inputDevice) {
    disconnect(m_inputDevice, &AudioDevice::volumeChanged, this, &VolumePopup::handleInputDeviceVolumeChanged);
    disconnect(m_inputDevice, &AudioDevice::muteChanged, this, &VolumePopup::handleInputDeviceMuteChanged);
    disconnect(m_inputDevice, &AudioDevice::nameChanged, this, &VolumePopup::updateStatusToolTip);
    disconnect(m_inputDevice, &AudioDevice::descriptionChanged, this, &VolumePopup::updateStatusToolTip);
    disconnect(m_inputDevice, &AudioDevice::enabledChanged, this, &VolumePopup::updateStatusToolTip);
  }

  m_inputDevice = device;
  m_inputSliderDragActive = false;
  m_hasDeferredBackendInputVolume = false;
  m_deferredBackendInputVolumePercent = 0;

  if (auto* input = m_inputDevice.data()) {
    m_inputMuteToggleButton->setChecked(input->mute());
    handleInputDeviceVolumeChanged(input->volume());
    connect(input, &AudioDevice::volumeChanged, this, &VolumePopup::handleInputDeviceVolumeChanged,
            Qt::QueuedConnection);
    connect(input, &AudioDevice::muteChanged, this, &VolumePopup::handleInputDeviceMuteChanged, Qt::QueuedConnection);
    connect(input, &AudioDevice::nameChanged, this, &VolumePopup::updateStatusToolTip, Qt::QueuedConnection);
    connect(input, &AudioDevice::descriptionChanged, this, &VolumePopup::updateStatusToolTip, Qt::QueuedConnection);
    connect(input, &AudioDevice::enabledChanged, this, &VolumePopup::updateStatusToolTip, Qt::QueuedConnection);
  }
  else {
    m_inputVolumeSlider->blockSignals(true);
    m_inputVolumeSlider->setValue(0);
    m_inputVolumeSlider->blockSignals(false);

    m_inputMuteToggleButton->setChecked(true);
  }

  updateControlAvailability();
  updateStatusToolTip();
}

void VolumePopup::setBackendAvailable(bool available, const QString& statusMessage) {
  const bool availabilityChanged = (m_backendAvailable != available);
  const QString normalizedStatus = available ? QString() : statusMessage.trimmed();
  const bool statusChanged = (m_backendStatusMessage != normalizedStatus);

  if (!availabilityChanged && !statusChanged) {
    return;
  }

  m_backendAvailable = available;
  m_backendStatusMessage = normalizedStatus;

  updateControlAvailability();
  updateStatusToolTip();
  updateStockIcon();
}

void VolumePopup::setSliderStep(int step) {
  m_volumeSlider->setSingleStep(step);
  m_volumeSlider->setPageStep(step * 10);
  m_inputVolumeSlider->setSingleStep(step);
  m_inputVolumeSlider->setPageStep(step * 10);
}

void VolumePopup::applyVolumeToSlider(int volume) {
  m_volumeSlider->blockSignals(true);
  m_volumeSlider->setValue(volume);
  m_volumeSlider->blockSignals(false);
  updateStatusToolTip();
}

void VolumePopup::applyInputVolumeToSlider(int volume) {
  m_inputVolumeSlider->blockSignals(true);
  m_inputVolumeSlider->setValue(volume);
  m_inputVolumeSlider->blockSignals(false);
  updateStatusToolTip();
}

void VolumePopup::updateControlAvailability() {
  const bool outputControlsEnabled = m_backendAvailable && m_device;
  m_volumeSlider->setEnabled(outputControlsEnabled);
  m_muteToggleButton->setEnabled(outputControlsEnabled);

  const bool inputControlsEnabled = m_backendAvailable && m_inputDevice;
  m_inputVolumeSlider->setEnabled(inputControlsEnabled);
  m_inputMuteToggleButton->setEnabled(inputControlsEnabled);

  const bool showInputControls = (m_inputDevice != nullptr);
  m_inputVolumeSlider->setVisible(showInputControls);
  m_inputMuteToggleButton->setVisible(showInputControls);
}

void VolumePopup::updateStatusToolTip() {
  QString outputTip;
  QString inputTip;

  if (!m_backendAvailable) {
    outputTip = tr("Audio backend unavailable");
    if (!m_backendStatusMessage.isEmpty()) {
      outputTip = tr("Audio backend unavailable: %1").arg(m_backendStatusMessage);
    }
    inputTip = outputTip;
  }
  else {
    if (m_device) {
      const QString outputName = tooltipDisplayName(m_device.data(), tr("Unknown output device"));
      const QString outputState = tooltipEndpointState(m_device.data(), tr("active"), tr("inactive"));
      const QString outputPercent = QStringLiteral("%1%").arg(m_volumeSlider->value());
      outputTip = tr("Output: %1 (%2), %3%4")
                      .arg(outputName, outputState, outputPercent, m_device->mute() ? tr(" (muted)") : QString());
    }
    else {
      outputTip = tr("No audio output device");
    }

    if (m_inputDevice) {
      const QString inputName = tooltipDisplayName(m_inputDevice.data(), tr("Unknown microphone input device"));
      const QString inputState = tooltipEndpointState(m_inputDevice.data(), tr("active"), tr("inactive"));
      const QString inputPercent = QStringLiteral("%1%").arg(m_inputVolumeSlider->value());
      inputTip = tr("Input: %1 (%2), %3%4")
                     .arg(inputName, inputState, inputPercent, m_inputDevice->mute() ? tr(" (muted)") : QString());
    }
    else {
      inputTip = tr("No microphone input device");
    }
  }

  m_volumeSlider->setToolTip(outputTip);
  m_inputVolumeSlider->setToolTip(inputTip);

  QString parentTip;
  if (!m_backendAvailable) {
    parentTip = outputTip;
  }
  else if (m_device && m_inputDevice) {
    parentTip = tr("%1\n%2").arg(outputTip, inputTip);
  }
  else if (m_inputDevice) {
    parentTip = inputTip;
  }
  else {
    parentTip = outputTip;
  }

  if (auto* parent = parentWidget()) {
    parent->setToolTip(parentTip);
  }
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
