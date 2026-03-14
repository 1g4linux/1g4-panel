/* plugin-volume/volumebutton.cpp
 * Volume control plugin implementation
 */

#include "volumebutton.h"

#include "audiodevice.h"
#include "volumepopup.h"

#include <QMouseEvent>
#include <QRect>
#include <QSizePolicy>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QWheelEvent>
#include <QtGlobal>

#include <OneG4/XdgIcon.h>

#include "../panel/ioneg4panel.h"
#include "../panel/ioneg4panelplugin.h"

VolumeButton::VolumeButton(IOneG4PanelPlugin* plugin, QWidget* parent)
    : QToolButton(parent), mPlugin(plugin), m_muteOnMiddleClick(true) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setAutoRaise(true);
  setMouseTracking(true);

  handleStockIconChanged(QStringLiteral("dialog-error"));

  m_volumePopup = new VolumePopup(this);

  m_popupHideTimer.setInterval(1000);
  connect(this, &VolumeButton::clicked, this, &VolumeButton::toggleVolumeSlider);
  connect(&m_popupHideTimer, &QTimer::timeout, this, &VolumeButton::hideVolumeSlider);

  connect(m_volumePopup, &VolumePopup::mouseEntered, &m_popupHideTimer, &QTimer::stop);
  connect(m_volumePopup, &VolumePopup::mouseLeft, this, [this] { m_popupHideTimer.start(); });

  connect(m_volumePopup, &VolumePopup::stockIconChanged, this, &VolumeButton::handleStockIconChanged);
}

VolumeButton::~VolumeButton() = default;

void VolumeButton::setMuteOnMiddleClick(bool state) {
  m_muteOnMiddleClick = state;
}

void VolumeButton::wheelEvent(QWheelEvent* event) {
  if (!m_volumePopup) {
    return;
  }

  m_volumePopup->handleWheelEvent(event);
}

void VolumeButton::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton && m_muteOnMiddleClick) {
    if (m_volumePopup && m_volumePopup->device()) {
      m_volumePopup->device()->toggleMute();
      return;
    }
  }

  QToolButton::mouseReleaseEvent(event);
}

void VolumeButton::toggleVolumeSlider() {
  if (!m_volumePopup) {
    return;
  }

  if (m_volumePopup->isVisible()) {
    hideVolumeSlider();
  }
  else {
    showVolumeSlider();
  }
}

void VolumeButton::showVolumeSlider() {
  if (!m_volumePopup || m_volumePopup->isVisible() || !mPlugin) {
    return;
  }

  m_popupHideTimer.stop();
  QToolTip::hideText();
  m_volumePopup->updateGeometry();
  m_volumePopup->adjustSize();

  const QRect pos = mPlugin->calculatePopupWindowPos(m_volumePopup->size());
  mPlugin->willShowWindow(m_volumePopup);
  m_volumePopup->openAt(pos.topLeft(), Qt::TopLeftCorner);
  m_volumePopup->raise();
}

void VolumeButton::hideVolumeSlider() {
  m_popupHideTimer.stop();

  if (m_volumePopup) {
    m_volumePopup->hide();
  }
}

void VolumeButton::handleStockIconChanged(const QString& iconName) {
  setIcon(XdgIcon::fromTheme(iconName));
}
