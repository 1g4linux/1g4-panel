if(NOT DEFINED ONEG4_PANEL_CMAKE)
    message(FATAL_ERROR "ONEG4_PANEL_CMAKE is required")
endif()
if(NOT DEFINED VOLUME_PLUGIN_CMAKE)
    message(FATAL_ERROR "VOLUME_PLUGIN_CMAKE is required")
endif()
if(NOT DEFINED ONEG4_VOLUME_SOURCE)
    message(FATAL_ERROR "ONEG4_VOLUME_SOURCE is required")
endif()
if(NOT DEFINED ONEG4_VOLUME_CONFIGURATION_SOURCE)
    message(FATAL_ERROR "ONEG4_VOLUME_CONFIGURATION_SOURCE is required")
endif()

file(READ "${ONEG4_PANEL_CMAKE}" ONEG4_PANEL_CMAKE_CONTENT)
file(READ "${VOLUME_PLUGIN_CMAKE}" VOLUME_PLUGIN_CMAKE_CONTENT)
file(READ "${ONEG4_VOLUME_SOURCE}" ONEG4_VOLUME_CONTENT)
file(READ "${ONEG4_VOLUME_CONFIGURATION_SOURCE}" ONEG4_VOLUME_CONFIGURATION_CONTENT)

foreach(OPTION_DECL
        "option(ONEG4_VOLUME_DEV_VERBOSE_LOGGING"
        "option(ONEG4_VOLUME_DEV_TEST_BACKENDS")
    string(FIND "${ONEG4_PANEL_CMAKE_CONTENT}" "${OPTION_DECL}" OPTION_DECL_POS)
    if(OPTION_DECL_POS EQUAL -1)
        message(FATAL_ERROR "Missing volume development option declaration: ${OPTION_DECL}")
    endif()
endforeach()

foreach(PLUGIN_CHECK
        "if(ONEG4_VOLUME_DEV_VERBOSE_LOGGING)"
        "ONEG4_VOLUME_DEV_VERBOSE_LOGGING=1"
        "if(ONEG4_VOLUME_DEV_TEST_BACKENDS)"
        "ONEG4_VOLUME_DEV_TEST_BACKENDS=1"
        "testaudioengine.cpp")
    string(FIND "${VOLUME_PLUGIN_CMAKE_CONTENT}" "${PLUGIN_CHECK}" PLUGIN_CHECK_POS)
    if(PLUGIN_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing plugin-volume development flag wiring: ${PLUGIN_CHECK}")
    endif()
endforeach()

foreach(SOURCE_CHECK
        "#ifdef ONEG4_VOLUME_DEV_VERBOSE_LOGGING"
        "QLoggingCategory::setFilterRules"
        "QLatin1String(\"TestBackend\")")
    string(FIND "${ONEG4_VOLUME_CONTENT}" "${SOURCE_CHECK}" SOURCE_CHECK_POS)
    if(SOURCE_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing volume runtime support for development flags: ${SOURCE_CHECK}")
    endif()
endforeach()

string(FIND "${ONEG4_VOLUME_CONFIGURATION_CONTENT}" "#ifdef ONEG4_VOLUME_DEV_TEST_BACKENDS" TEST_BACKEND_UI_POS)
if(TEST_BACKEND_UI_POS EQUAL -1)
    message(FATAL_ERROR "Test backend option is not exposed in the configuration UI")
endif()
