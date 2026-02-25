if(NOT DEFINED ONEG4_PANEL_CMAKE)
    message(FATAL_ERROR "ONEG4_PANEL_CMAKE is required")
endif()

file(READ "${ONEG4_PANEL_CMAKE}" ONEG4_PANEL_CMAKE_CONTENT)

foreach(REQUIRED_QT_COMPONENT
        "find_package(Qt6DBus"
        "find_package(Qt6Widgets"
        "find_package(Qt6Xml")
    string(FIND "${ONEG4_PANEL_CMAKE_CONTENT}" "${REQUIRED_QT_COMPONENT}" QT_COMPONENT_POS)
    if(QT_COMPONENT_POS EQUAL -1)
        message(FATAL_ERROR "Missing required Qt dependency check: ${REQUIRED_QT_COMPONENT}")
    endif()
endforeach()

foreach(REQUIRED_CHECK
        "pkg_check_modules(PIPEWIRE QUIET libpipewire-0.3)"
        "pkg_check_modules(WIREPLUMBER QUIET wireplumber-0.5)"
        "pkg_check_modules(BLUEZ QUIET bluez)")
    string(FIND "${ONEG4_PANEL_CMAKE_CONTENT}" "${REQUIRED_CHECK}" CHECK_POS)
    if(CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing dependency probe: ${REQUIRED_CHECK}")
    endif()
endforeach()

foreach(REQUIRED_ERROR
        "Volume plugin requires PipeWire development files (pkg-config module: libpipewire-0.3)"
        "Volume plugin requires WirePlumber development files (pkg-config module: wireplumber-0.5)"
        "Volume plugin requires BlueZ development files (pkg-config module: bluez)")
    string(FIND "${ONEG4_PANEL_CMAKE_CONTENT}" "${REQUIRED_ERROR}" ERROR_POS)
    if(ERROR_POS EQUAL -1)
        message(FATAL_ERROR "Missing clear configure-time error message: ${REQUIRED_ERROR}")
    endif()
endforeach()
