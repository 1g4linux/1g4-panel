if(NOT DEFINED ONEG4_VOLUME_SOURCE OR NOT EXISTS "${ONEG4_VOLUME_SOURCE}")
  message(FATAL_ERROR "Missing ONEG4_VOLUME_SOURCE")
endif()

if(NOT DEFINED VOLUME_POPUP_SOURCE OR NOT EXISTS "${VOLUME_POPUP_SOURCE}")
  message(FATAL_ERROR "Missing VOLUME_POPUP_SOURCE")
endif()

file(READ "${ONEG4_VOLUME_SOURCE}" ONEG4_VOLUME_CONTENT)
file(READ "${VOLUME_POPUP_SOURCE}" VOLUME_POPUP_CONTENT)

set(REQUIRED_ENGINE_CONNECTION
    "connect(m_engine, &AudioEngine::sinkListChanged, this, &OneG4Volume::handleSinkListChanged, Qt::QueuedConnection);")
string(FIND "${ONEG4_VOLUME_CONTENT}" "${REQUIRED_ENGINE_CONNECTION}" ENGINE_POS)
if(ENGINE_POS EQUAL -1)
  message(FATAL_ERROR "OneG4Volume sinkListChanged connection must use Qt::QueuedConnection.")
endif()

set(REQUIRED_NOTIFICATION_VOLUME_CONNECTION
    "connect(sink, &AudioDevice::volumeChanged, this, [this] { showNotification(); }, Qt::QueuedConnection);")
string(FIND "${ONEG4_VOLUME_CONTENT}" "${REQUIRED_NOTIFICATION_VOLUME_CONNECTION}" NOTIFY_VOLUME_POS)
if(NOTIFY_VOLUME_POS EQUAL -1)
  message(FATAL_ERROR "OneG4Volume volume notification connection must use Qt::QueuedConnection.")
endif()

set(REQUIRED_NOTIFICATION_MUTE_CONNECTION
    "connect(sink, &AudioDevice::muteChanged, this, [this] { showNotification(); }, Qt::QueuedConnection);")
string(FIND "${ONEG4_VOLUME_CONTENT}" "${REQUIRED_NOTIFICATION_MUTE_CONNECTION}" NOTIFY_MUTE_POS)
if(NOTIFY_MUTE_POS EQUAL -1)
  message(FATAL_ERROR "OneG4Volume mute notification connection must use Qt::QueuedConnection.")
endif()

set(REQUIRED_POPUP_VOLUME_CONNECTION
    "connect(dev, &AudioDevice::volumeChanged, this, &VolumePopup::handleDeviceVolumeChanged, Qt::QueuedConnection);")
string(FIND "${VOLUME_POPUP_CONTENT}" "${REQUIRED_POPUP_VOLUME_CONNECTION}" POPUP_VOLUME_POS)
if(POPUP_VOLUME_POS EQUAL -1)
  message(FATAL_ERROR "VolumePopup volumeChanged connection must use Qt::QueuedConnection.")
endif()

set(REQUIRED_POPUP_MUTE_CONNECTION
    "connect(dev, &AudioDevice::muteChanged, this, &VolumePopup::handleDeviceMuteChanged, Qt::QueuedConnection);")
string(FIND "${VOLUME_POPUP_CONTENT}" "${REQUIRED_POPUP_MUTE_CONNECTION}" POPUP_MUTE_POS)
if(POPUP_MUTE_POS EQUAL -1)
  message(FATAL_ERROR "VolumePopup muteChanged connection must use Qt::QueuedConnection.")
endif()

string(FIND "${ONEG4_VOLUME_CONTENT}" "if (QThread::currentThread() != thread())" ONEG4_THREAD_GUARD_POS)
if(ONEG4_THREAD_GUARD_POS EQUAL -1)
  message(FATAL_ERROR "OneG4Volume must guard handleSinkListChanged to run on its owning thread.")
endif()

string(FIND "${VOLUME_POPUP_CONTENT}" "if (QThread::currentThread() != thread())" POPUP_THREAD_GUARD_POS)
if(POPUP_THREAD_GUARD_POS EQUAL -1)
  message(FATAL_ERROR "VolumePopup must guard backend update slots to run on the widget thread.")
endif()
