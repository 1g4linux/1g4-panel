/* plugin-volume/volumelogging.cpp
 * Shared logging categories for the volume plugin.
 */

#include "volumelogging.h"

Q_LOGGING_CATEGORY(lcVolumeUi, "oneg4.panel.plugin.volume.ui", QtWarningMsg)
Q_LOGGING_CATEGORY(lcVolumeBackend, "oneg4.panel.plugin.volume.backend", QtWarningMsg)
Q_LOGGING_CATEGORY(lcVolumeBluetooth, "oneg4.panel.plugin.volume.bluetooth", QtWarningMsg)
Q_LOGGING_CATEGORY(lcVolumeRouting, "oneg4.panel.plugin.volume.routing", QtWarningMsg)
Q_LOGGING_CATEGORY(lcVolumePersistence, "oneg4.panel.plugin.volume.persistence", QtWarningMsg)
