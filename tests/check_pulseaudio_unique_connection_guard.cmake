if(NOT DEFINED PULSEAUDIO_ENGINE_SOURCE)
    message(FATAL_ERROR "PULSEAUDIO_ENGINE_SOURCE is required")
endif()

file(READ "${PULSEAUDIO_ENGINE_SOURCE}" PULSEAUDIO_ENGINE_CONTENT)

string(FIND "${PULSEAUDIO_ENGINE_CONTENT}" "&PulseAudioEngine::sinkInfoChanged" SIGNAL_POS)
if(SIGNAL_POS EQUAL -1)
    message(FATAL_ERROR "sinkInfoChanged signal usage was not found")
endif()

string(FIND "${PULSEAUDIO_ENGINE_CONTENT}" "&PulseAudioEngine::retrieveSinkInfo" SLOT_POS)
if(SLOT_POS EQUAL -1)
    message(FATAL_ERROR "retrieveSinkInfo slot usage was not found")
endif()

set(UNIQUE_CONNECTION_EXPR "static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection)")
string(FIND "${PULSEAUDIO_ENGINE_CONTENT}" "${UNIQUE_CONNECTION_EXPR}" UNIQUE_POS)
if(UNIQUE_POS EQUAL -1)
    message(FATAL_ERROR "Expected unique queued connection expression is missing")
endif()
