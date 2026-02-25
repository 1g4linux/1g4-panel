if(NOT DEFINED PIPEWIRE_ENGINE_SOURCE OR NOT EXISTS "${PIPEWIRE_ENGINE_SOURCE}")
  message(FATAL_ERROR "Missing PIPEWIRE_ENGINE_SOURCE")
endif()

if(NOT DEFINED PIPEWIRE_ENGINE_HEADER OR NOT EXISTS "${PIPEWIRE_ENGINE_HEADER}")
  message(FATAL_ERROR "Missing PIPEWIRE_ENGINE_HEADER")
endif()

file(READ "${PIPEWIRE_ENGINE_SOURCE}" PIPEWIRE_ENGINE_CONTENT)
file(READ "${PIPEWIRE_ENGINE_HEADER}" PIPEWIRE_ENGINE_HEADER_CONTENT)

string(FIND "${PIPEWIRE_ENGINE_HEADER_CONTENT}" "std::atomic_bool m_isShuttingDown;" SHUTDOWN_FLAG_POS)
if(SHUTDOWN_FLAG_POS EQUAL -1)
  message(FATAL_ERROR "PipeWireEngine must declare an atomic shutdown guard for queued callback safety.")
endif()

string(FIND "${PIPEWIRE_ENGINE_CONTENT}" "m_isShuttingDown.store(true, std::memory_order_release);" SHUTDOWN_STORE_POS)
if(SHUTDOWN_STORE_POS EQUAL -1)
  message(FATAL_ERROR "PipeWireEngine destructor must publish shutdown before teardown.")
endif()

string(REGEX MATCHALL "if \\(engine->isShuttingDown\\(\\)\\)" CALLBACK_THREAD_GUARDS "${PIPEWIRE_ENGINE_CONTENT}")
list(LENGTH CALLBACK_THREAD_GUARDS CALLBACK_THREAD_GUARD_COUNT)
if(CALLBACK_THREAD_GUARD_COUNT LESS 4)
  message(FATAL_ERROR "Expected PipeWire callbacks to guard against shutdown before queueing updates.")
endif()

string(REGEX MATCHALL "enginePtr->isShuttingDown\\(\\)" QUEUED_LAMBDA_GUARDS "${PIPEWIRE_ENGINE_CONTENT}")
list(LENGTH QUEUED_LAMBDA_GUARDS QUEUED_LAMBDA_GUARD_COUNT)
if(QUEUED_LAMBDA_GUARD_COUNT LESS 5)
  message(FATAL_ERROR "Expected queued PipeWire lambdas to re-check shutdown before applying updates.")
endif()
