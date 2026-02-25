if(NOT DEFINED VOLUME_BACKEND_TARGET_DOC)
    message(FATAL_ERROR "VOLUME_BACKEND_TARGET_DOC is required")
endif()
if(NOT DEFINED ONEG4_PANEL_CMAKE)
    message(FATAL_ERROR "ONEG4_PANEL_CMAKE is required")
endif()

file(READ "${VOLUME_BACKEND_TARGET_DOC}" VOLUME_BACKEND_TARGET_DOC_CONTENT)
file(READ "${ONEG4_PANEL_CMAKE}" ONEG4_PANEL_CMAKE_CONTENT)

foreach(DOC_CHECK
        "Primary control path: PipeWire + WirePlumber + BlueZ."
        "Minimum supported PipeWire version: 0.3.0."
        "Minimum supported WirePlumber version: 0.5.0."
        "Minimum supported BlueZ version: 5.0."
        "No PulseAudio backend in the final control path."
        "No standalone PulseAudio backend selection in plugin configuration."
        "No PulseAudio-specific UX terminology where generic audio terms are sufficient."
        "The panel \"Mixer\" action opens the built-in mixer dialog (not an external process lookup)."
        "The built-in mixer dialog links libpulse/libpulse-mainloop-glib for compatibility with PulseAudio and PipeWire's pulse server."
        "Capability detection behavior contract"
        "Missing PipeWire runtime connection:"
        "Missing WirePlumber policy capability:"
        "Missing BlueZ battery/profile/port properties:"
        "built-in mixer dialog uses libpulse compatibility libraries.")
    string(FIND "${VOLUME_BACKEND_TARGET_DOC_CONTENT}" "${DOC_CHECK}" DOC_CHECK_POS)
    if(DOC_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing backend target matrix documentation snippet: ${DOC_CHECK}")
    endif()
endforeach()

foreach(CMAKE_CHECK
        "set(ONEG4_VOLUME_MIN_PIPEWIRE_VERSION \"0.3.0\")"
        "set(ONEG4_VOLUME_MIN_WIREPLUMBER_VERSION \"0.5.0\")"
        "set(ONEG4_VOLUME_MIN_BLUEZ_VERSION \"5.0\")")
    string(FIND "${ONEG4_PANEL_CMAKE_CONTENT}" "${CMAKE_CHECK}" CMAKE_CHECK_POS)
    if(CMAKE_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing build-time minimum version declaration: ${CMAKE_CHECK}")
    endif()
endforeach()
