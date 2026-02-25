if(NOT DEFINED VOLUME_PLUGIN_SO)
    message(FATAL_ERROR "VOLUME_PLUGIN_SO is required")
endif()

if(NOT EXISTS "${VOLUME_PLUGIN_SO}")
    message(FATAL_ERROR "Volume plugin shared object does not exist: ${VOLUME_PLUGIN_SO}")
endif()

execute_process(
    COMMAND ldd "${VOLUME_PLUGIN_SO}"
    RESULT_VARIABLE LDD_EXIT_CODE
    OUTPUT_VARIABLE LDD_OUTPUT
    ERROR_VARIABLE LDD_ERROR
)

if(NOT LDD_EXIT_CODE EQUAL 0)
    message(FATAL_ERROR "ldd failed for ${VOLUME_PLUGIN_SO}: ${LDD_ERROR}")
endif()

foreach(REQUIRED_RUNTIME_DEP
        "libpulse.so"
        "libpulse-mainloop-glib.so")
    string(FIND "${LDD_OUTPUT}" "${REQUIRED_RUNTIME_DEP}" REQUIRED_RUNTIME_DEP_POS)
    if(REQUIRED_RUNTIME_DEP_POS EQUAL -1)
        message(FATAL_ERROR "Built-in mixer runtime dependency is missing from volume plugin linkage: ${REQUIRED_RUNTIME_DEP}")
    endif()
endforeach()
