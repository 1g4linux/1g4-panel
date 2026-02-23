if(NOT DEFINED PIPEWIRE_ENGINE_SOURCE)
    message(FATAL_ERROR "PIPEWIRE_ENGINE_SOURCE is required")
endif()

file(READ "${PIPEWIRE_ENGINE_SOURCE}" PIPEWIRE_ENGINE_CONTENT)

string(FIND "${PIPEWIRE_ENGINE_CONTENT}" "qDebug(" QDEBUG_POS)
if(NOT QDEBUG_POS EQUAL -1)
    message(FATAL_ERROR "Ungated qDebug() calls remain in PipeWire engine")
endif()

set(LOGGING_CATEGORY_DECL "Q_LOGGING_CATEGORY(lcPipeWireEngine, \"oneg4.panel.plugin.volume.pipewire\", QtWarningMsg)")
string(FIND "${PIPEWIRE_ENGINE_CONTENT}" "${LOGGING_CATEGORY_DECL}" CATEGORY_POS)
if(CATEGORY_POS EQUAL -1)
    message(FATAL_ERROR "Expected PipeWire logging category declaration is missing")
endif()

string(FIND "${PIPEWIRE_ENGINE_CONTENT}" "qCDebug(lcPipeWireEngine)" CDEBUG_POS)
if(CDEBUG_POS EQUAL -1)
    message(FATAL_ERROR "PipeWire debug logs are not routed through qCDebug(lcPipeWireEngine)")
endif()
