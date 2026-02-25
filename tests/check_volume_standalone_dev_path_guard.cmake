if(NOT DEFINED ONEG4_PANEL_CMAKE)
    message(FATAL_ERROR "ONEG4_PANEL_CMAKE is required")
endif()

file(READ "${ONEG4_PANEL_CMAKE}" ONEG4_PANEL_CMAKE_CONTENT)

foreach(REQUIRED_SNIPPET
        "option(ONEG4_DEV_VOLUME_ONLY"
        "if(ONEG4_DEV_VOLUME_ONLY)"
        "setByDefault(TASKBAR_PLUGIN No)"
        "setByDefault(STATUSNOTIFIER_PLUGIN No)"
        "setByDefault(WORLDCLOCK_PLUGIN No)"
        "setByDefault(SPACER_PLUGIN No)"
        "setByDefault(VOLUME_PLUGIN Yes)"
        "if(NOT ONEG4_DEV_VOLUME_ONLY)"
        "add_subdirectory(panel)"
        "add_subdirectory(autostart)")
    string(FIND "${ONEG4_PANEL_CMAKE_CONTENT}" "${REQUIRED_SNIPPET}" REQUIRED_SNIPPET_POS)
    if(REQUIRED_SNIPPET_POS EQUAL -1)
        message(FATAL_ERROR "Missing standalone volume development path contract: ${REQUIRED_SNIPPET}")
    endif()
endforeach()
