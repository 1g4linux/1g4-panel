if(NOT DEFINED ONEG4_PANEL_CMAKE)
    message(FATAL_ERROR "ONEG4_PANEL_CMAKE is required")
endif()
if(NOT DEFINED VOLUME_PLUGIN_CMAKE)
    message(FATAL_ERROR "VOLUME_PLUGIN_CMAKE is required")
endif()

file(READ "${ONEG4_PANEL_CMAKE}" ONEG4_PANEL_CMAKE_CONTENT)
file(READ "${VOLUME_PLUGIN_CMAKE}" VOLUME_PLUGIN_CMAKE_CONTENT)

foreach(TOPLEVEL_CHECK
        "if(VOLUME_PLUGIN)"
        "add_subdirectory(plugin-volume)")
    string(FIND "${ONEG4_PANEL_CMAKE_CONTENT}" "${TOPLEVEL_CHECK}" TOPLEVEL_CHECK_POS)
    if(TOPLEVEL_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing top-level volume plugin integration: ${TOPLEVEL_CHECK}")
    endif()
endforeach()

string(FIND "${VOLUME_PLUGIN_CMAKE_CONTENT}" "pkg_check_modules(PIPEWIRE" DUPLICATE_PIPEWIRE_CHECK_POS)
if(NOT DUPLICATE_PIPEWIRE_CHECK_POS EQUAL -1)
    message(FATAL_ERROR "plugin-volume/CMakeLists.txt should consume top-level PIPEWIRE detection instead of re-probing")
endif()

foreach(FORBIDDEN_SUBDIR_CHECK
        "pkg_check_modules(PULSEAUDIO"
        "pulseaudioengine.cpp"
        "pulseaudioengine.h")
    string(FIND "${VOLUME_PLUGIN_CMAKE_CONTENT}" "${FORBIDDEN_SUBDIR_CHECK}" FORBIDDEN_SUBDIR_CHECK_POS)
    if(NOT FORBIDDEN_SUBDIR_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Unexpected PulseAudio/libpulse integration remains in plugin-volume/CMakeLists.txt: ${FORBIDDEN_SUBDIR_CHECK}")
    endif()
endforeach()

foreach(SUBDIR_CHECK
        "if(NOT PIPEWIRE_FOUND)"
        "include_directories(SYSTEM \${PIPEWIRE_INCLUDE_DIRS})"
        "set(LIBRARIES \${LIBRARIES} \${PIPEWIRE_LIBRARIES})"
        "set(SOURCES \${SOURCES} pipewireengine.cpp)"
        "set(HEADERS \${HEADERS} pipewireengine.h)"
        "add_subdirectory(1g4-mixer)"
        "set(LIBRARIES \${LIBRARIES} 1g4-mixer)")
    string(FIND "${VOLUME_PLUGIN_CMAKE_CONTENT}" "${SUBDIR_CHECK}" SUBDIR_CHECK_POS)
    if(SUBDIR_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing subdirectory integration contract: ${SUBDIR_CHECK}")
    endif()
endforeach()
