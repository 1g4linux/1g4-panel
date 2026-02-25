/* plugin-volume/volumelogging.h
 * Shared logging categories for the volume plugin.
 */

#ifndef ONEG4VOLUMELOGGING_H
#define ONEG4VOLUMELOGGING_H

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcVolumeUi)
Q_DECLARE_LOGGING_CATEGORY(lcVolumeBackend)
Q_DECLARE_LOGGING_CATEGORY(lcVolumeBluetooth)
Q_DECLARE_LOGGING_CATEGORY(lcVolumeRouting)
Q_DECLARE_LOGGING_CATEGORY(lcVolumePersistence)

#endif  // ONEG4VOLUMELOGGING_H
