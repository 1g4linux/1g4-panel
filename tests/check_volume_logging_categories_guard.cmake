if(NOT DEFINED ONEG4_VOLUME_LOGGING_HEADER)
    message(FATAL_ERROR "ONEG4_VOLUME_LOGGING_HEADER is required")
endif()
if(NOT DEFINED ONEG4_VOLUME_LOGGING_SOURCE)
    message(FATAL_ERROR "ONEG4_VOLUME_LOGGING_SOURCE is required")
endif()
if(NOT DEFINED ONEG4_VOLUME_SOURCE)
    message(FATAL_ERROR "ONEG4_VOLUME_SOURCE is required")
endif()
if(NOT DEFINED PIPEWIRE_ENGINE_SOURCE)
    message(FATAL_ERROR "PIPEWIRE_ENGINE_SOURCE is required")
endif()
if(NOT DEFINED WIREPLUMBER_POLICY_SOURCE)
    message(FATAL_ERROR "WIREPLUMBER_POLICY_SOURCE is required")
endif()

file(READ "${ONEG4_VOLUME_LOGGING_HEADER}" ONEG4_VOLUME_LOGGING_HEADER_CONTENT)
file(READ "${ONEG4_VOLUME_LOGGING_SOURCE}" ONEG4_VOLUME_LOGGING_SOURCE_CONTENT)
file(READ "${ONEG4_VOLUME_SOURCE}" ONEG4_VOLUME_CONTENT)
file(READ "${PIPEWIRE_ENGINE_SOURCE}" PIPEWIRE_ENGINE_CONTENT)
file(READ "${WIREPLUMBER_POLICY_SOURCE}" WIREPLUMBER_POLICY_CONTENT)

foreach(CATEGORY_DECL
        "Q_DECLARE_LOGGING_CATEGORY(lcVolumeUi)"
        "Q_DECLARE_LOGGING_CATEGORY(lcVolumeBackend)"
        "Q_DECLARE_LOGGING_CATEGORY(lcVolumeBluetooth)"
        "Q_DECLARE_LOGGING_CATEGORY(lcVolumeRouting)"
        "Q_DECLARE_LOGGING_CATEGORY(lcVolumePersistence)")
    string(FIND "${ONEG4_VOLUME_LOGGING_HEADER_CONTENT}" "${CATEGORY_DECL}" CATEGORY_DECL_POS)
    if(CATEGORY_DECL_POS EQUAL -1)
        message(FATAL_ERROR "Missing logging category declaration '${CATEGORY_DECL}'")
    endif()
endforeach()

foreach(CATEGORY_DEF
        "Q_LOGGING_CATEGORY(lcVolumeUi, \"oneg4.panel.plugin.volume.ui\", QtWarningMsg)"
        "Q_LOGGING_CATEGORY(lcVolumeBackend, \"oneg4.panel.plugin.volume.backend\", QtWarningMsg)"
        "Q_LOGGING_CATEGORY(lcVolumeBluetooth, \"oneg4.panel.plugin.volume.bluetooth\", QtWarningMsg)"
        "Q_LOGGING_CATEGORY(lcVolumeRouting, \"oneg4.panel.plugin.volume.routing\", QtWarningMsg)"
        "Q_LOGGING_CATEGORY(lcVolumePersistence, \"oneg4.panel.plugin.volume.persistence\", QtWarningMsg)")
    string(FIND "${ONEG4_VOLUME_LOGGING_SOURCE_CONTENT}" "${CATEGORY_DEF}" CATEGORY_DEF_POS)
    if(CATEGORY_DEF_POS EQUAL -1)
        message(FATAL_ERROR "Missing logging category definition '${CATEGORY_DEF}'")
    endif()
endforeach()

string(FIND "${ONEG4_VOLUME_CONTENT}" "qCDebug(lcVolumeBluetooth)" VOLUME_BLUETOOTH_DEBUG_POS)
if(VOLUME_BLUETOOTH_DEBUG_POS EQUAL -1)
    message(FATAL_ERROR "Expected qCDebug(lcVolumeBluetooth) usage in volume plugin entrypoint is missing")
endif()

string(FIND "${PIPEWIRE_ENGINE_CONTENT}" "qCWarning(lcVolumeBackend)" PW_BACKEND_WARNING_POS)
if(PW_BACKEND_WARNING_POS EQUAL -1)
    message(FATAL_ERROR "Expected qCWarning(lcVolumeBackend) usage in PipeWire engine is missing")
endif()

string(FIND "${PIPEWIRE_ENGINE_CONTENT}" "qCDebug(lcVolumeBluetooth)" BLUETOOTH_DEBUG_POS)
if(BLUETOOTH_DEBUG_POS EQUAL -1)
    message(FATAL_ERROR "Expected qCDebug(lcVolumeBluetooth) usage is missing")
endif()

string(FIND "${PIPEWIRE_ENGINE_CONTENT}" "qCDebug(lcVolumeRouting)" ROUTING_DEBUG_POS)
if(ROUTING_DEBUG_POS EQUAL -1)
    message(FATAL_ERROR "Expected qCDebug(lcVolumeRouting) usage is missing")
endif()

string(FIND "${WIREPLUMBER_POLICY_CONTENT}" "qCWarning(lcVolumePersistence)" PERSISTENCE_WARNING_POS)
if(PERSISTENCE_WARNING_POS EQUAL -1)
    message(FATAL_ERROR "Expected qCWarning(lcVolumePersistence) usage is missing")
endif()

foreach(FILE_CONTENT
        ONEG4_VOLUME_CONTENT
        PIPEWIRE_ENGINE_CONTENT
        WIREPLUMBER_POLICY_CONTENT)
    string(FIND "${${FILE_CONTENT}}" "qWarning(" RAW_WARNING_POS)
    if(NOT RAW_WARNING_POS EQUAL -1)
        message(FATAL_ERROR "Ungated qWarning() call remains in '${FILE_CONTENT}'")
    endif()
endforeach()
