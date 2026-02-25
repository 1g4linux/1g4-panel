if(NOT DEFINED ONEG4_VOLUME_SOURCE)
    message(FATAL_ERROR "ONEG4_VOLUME_SOURCE is required")
endif()

file(READ "${ONEG4_VOLUME_SOURCE}" ONEG4_VOLUME_CONTENT)

string(FIND "${ONEG4_VOLUME_CONTENT}" "#elif USE_PIPEWIRE" LEGACY_GUARD_POS)
if(NOT LEGACY_GUARD_POS EQUAL -1)
    message(FATAL_ERROR "Legacy guard '#elif USE_PIPEWIRE' is present")
endif()

string(FIND "${ONEG4_VOLUME_CONTENT}" "USE_PULSEAUDIO" LEGACY_PULSEAUDIO_GUARD_POS)
if(NOT LEGACY_PULSEAUDIO_GUARD_POS EQUAL -1)
    message(FATAL_ERROR "Legacy PulseAudio preprocessor guard is present")
endif()

string(FIND "${ONEG4_VOLUME_CONTENT}" "#ifdef USE_PIPEWIRE" PIPEWIRE_GUARD_POS)
if(PIPEWIRE_GUARD_POS EQUAL -1)
    message(FATAL_ERROR "Expected PipeWire guard '#ifdef USE_PIPEWIRE' is missing")
endif()

string(FIND "${ONEG4_VOLUME_CONTENT}" "engine = new PipeWireEngine(this);" PIPEWIRE_ENGINE_FALLBACK_POS)
if(PIPEWIRE_ENGINE_FALLBACK_POS EQUAL -1)
    message(FATAL_ERROR "Expected PipeWire fallback engine construction is missing")
endif()
