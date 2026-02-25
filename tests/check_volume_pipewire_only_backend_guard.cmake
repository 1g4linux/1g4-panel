if(NOT DEFINED VOLUME_PLUGIN_CMAKE)
    message(FATAL_ERROR "VOLUME_PLUGIN_CMAKE is required")
endif()
if(NOT DEFINED ONEG4_VOLUME_SOURCE)
    message(FATAL_ERROR "ONEG4_VOLUME_SOURCE is required")
endif()
if(NOT DEFINED ONEG4_VOLUME_HEADER)
    message(FATAL_ERROR "ONEG4_VOLUME_HEADER is required")
endif()
if(NOT DEFINED ONEG4_VOLUME_CONFIGURATION_SOURCE)
    message(FATAL_ERROR "ONEG4_VOLUME_CONFIGURATION_SOURCE is required")
endif()

file(READ "${VOLUME_PLUGIN_CMAKE}" VOLUME_PLUGIN_CMAKE_CONTENT)
file(READ "${ONEG4_VOLUME_SOURCE}" ONEG4_VOLUME_SOURCE_CONTENT)
file(READ "${ONEG4_VOLUME_HEADER}" ONEG4_VOLUME_HEADER_CONTENT)
file(READ "${ONEG4_VOLUME_CONFIGURATION_SOURCE}" ONEG4_VOLUME_CONFIGURATION_SOURCE_CONTENT)

foreach(FORBIDDEN_PLUGIN_CMAKE_SNIPPET
        "pkg_check_modules(PULSEAUDIO"
        "USE_PULSEAUDIO"
        "pulseaudioengine.cpp"
        "pulseaudioengine.h"
        "add_subdirectory(1g4-mixer)"
        "set(LIBRARIES \${LIBRARIES} 1g4-mixer)")
    string(FIND "${VOLUME_PLUGIN_CMAKE_CONTENT}" "${FORBIDDEN_PLUGIN_CMAKE_SNIPPET}" FORBIDDEN_PLUGIN_CMAKE_POS)
    if(NOT FORBIDDEN_PLUGIN_CMAKE_POS EQUAL -1)
        message(FATAL_ERROR "PulseAudio/libpulse build path still present: ${FORBIDDEN_PLUGIN_CMAKE_SNIPPET}")
    endif()
endforeach()

foreach(FORBIDDEN_VOLUME_SOURCE_SNIPPET
        "pulseaudioengine.h"
        "1g4-mixer.h"
        "QLatin1String(\"PulseAudio\")"
        "new PulseAudioEngine("
        "void OneG4Volume::openMixer()")
    string(FIND "${ONEG4_VOLUME_SOURCE_CONTENT}" "${FORBIDDEN_VOLUME_SOURCE_SNIPPET}" FORBIDDEN_VOLUME_SOURCE_POS)
    if(NOT FORBIDDEN_VOLUME_SOURCE_POS EQUAL -1)
        message(FATAL_ERROR "PulseAudio/libpulse runtime path still present in oneg4volume.cpp: ${FORBIDDEN_VOLUME_SOURCE_SNIPPET}")
    endif()
endforeach()

string(FIND "${ONEG4_VOLUME_HEADER_CONTENT}" "void openMixer();" OPEN_MIXER_DECL_POS)
if(NOT OPEN_MIXER_DECL_POS EQUAL -1)
    message(FATAL_ERROR "Open mixer API should not be present in PipeWire-only backend mode")
endif()

foreach(FORBIDDEN_CONFIGURATION_SNIPPET
        "QLatin1String(\"PulseAudio\")"
        "#ifdef USE_PULSEAUDIO")
    string(FIND "${ONEG4_VOLUME_CONFIGURATION_SOURCE_CONTENT}" "${FORBIDDEN_CONFIGURATION_SNIPPET}" FORBIDDEN_CONFIGURATION_POS)
    if(NOT FORBIDDEN_CONFIGURATION_POS EQUAL -1)
        message(FATAL_ERROR "PulseAudio backend option still present in configuration source: ${FORBIDDEN_CONFIGURATION_SNIPPET}")
    endif()
endforeach()
