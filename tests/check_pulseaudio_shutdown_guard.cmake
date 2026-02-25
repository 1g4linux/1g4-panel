if(NOT DEFINED PULSEAUDIO_ENGINE_SOURCE)
    message(FATAL_ERROR "PULSEAUDIO_ENGINE_SOURCE is required")
endif()

if(NOT DEFINED PULSEAUDIO_ENGINE_HEADER)
    message(FATAL_ERROR "PULSEAUDIO_ENGINE_HEADER is required")
endif()

file(READ "${PULSEAUDIO_ENGINE_SOURCE}" PULSEAUDIO_ENGINE_CONTENT)
file(READ "${PULSEAUDIO_ENGINE_HEADER}" PULSEAUDIO_ENGINE_HEADER_CONTENT)

set(REQUIRED_HEADER_SNIPPETS
    "void shutdownContext();"
    "std::atomic_bool m_shuttingDown;"
)

foreach(SNIPPET IN LISTS REQUIRED_HEADER_SNIPPETS)
    string(FIND "${PULSEAUDIO_ENGINE_HEADER_CONTENT}" "${SNIPPET}" POS)
    if(POS EQUAL -1)
        message(FATAL_ERROR "Missing PulseAudio shutdown guard declaration: ${SNIPPET}")
    endif()
endforeach()

set(REQUIRED_SOURCE_SNIPPETS
    "m_shuttingDown.store(true, std::memory_order_release);"
    "shutdownContext();"
    "void PulseAudioEngine::shutdownContext()"
    "m_reconnectionTimer.stop();"
    "pa_context_set_state_callback(m_context, nullptr, nullptr);"
    "pa_context_set_event_callback(m_context, nullptr, nullptr);"
    "pa_context_set_subscribe_callback(m_context, nullptr, nullptr);"
    "pa_context_disconnect(m_context);"
    "pa_context_unref(m_context);"
    "if (isShuttingDown())"
)

foreach(SNIPPET IN LISTS REQUIRED_SOURCE_SNIPPETS)
    string(FIND "${PULSEAUDIO_ENGINE_CONTENT}" "${SNIPPET}" POS)
    if(POS EQUAL -1)
        message(FATAL_ERROR "Missing PulseAudio shutdown guard implementation snippet: ${SNIPPET}")
    endif()
endforeach()
