cmake_minimum_required(VERSION 3.18.0 FATAL_ERROR)
# CMP0000: Call the cmake_minimum_required() command at the beginning of the top-level
# CMakeLists.txt file even before calling the project() command.
# The cmake_minimum_required(VERSION) command implicitly invokes the cmake_policy(VERSION)
# command to specify that the current project code is written for the given range of CMake
# versions.
project(1g4-panel)
include(CTest)

option(WITH_SCREENSAVER_FALLBACK "Include support for converting the deprecated 'screensaver' plugin to 'quicklaunch'. This requires the oneg4-leave (oneg4-session) to be installed in runtime." ON)

# additional cmake files
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

macro(setByDefault VAR_NAME VAR_VALUE)
  if(NOT DEFINED ${VAR_NAME})
    set (${VAR_NAME} ${VAR_VALUE})
  endif(NOT DEFINED ${VAR_NAME})
endmacro()

setByDefault(ONEG4_ETC_XDG_DIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg")

include(GNUInstallDirs)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(REQUIRED_QT_VERSION "6.6.0")
set(KF6_MINIMUM_VERSION "6.0.0")


find_package(Qt6DBus ${REQUIRED_QT_VERSION} REQUIRED)
find_package(Qt6Widgets ${REQUIRED_QT_VERSION} REQUIRED)
find_package(Qt6Xml ${REQUIRED_QT_VERSION} REQUIRED)
find_package(KF6WindowSystem ${KF6_MINIMUM_VERSION} REQUIRED)

set(ONEG4_COMPAT_SOURCES
    OneG4/Application.cpp
    OneG4/ConfigDialog.cpp
    OneG4/GridLayout.cpp
    OneG4/HtmlDelegate.cpp
    OneG4/Notification.cpp
    OneG4/PluginInfo.cpp
    OneG4/RotatedWidget.cpp
    OneG4/Settings.cpp
    OneG4/XdgIcon.cpp
    OneG4/XdgDirs.cpp
)

set(ONEG4_COMPAT_HEADERS
    OneG4/Application.h
    OneG4/ConfigDialog.h
    OneG4/GridLayout.h
    OneG4/HtmlDelegate.h
    OneG4/Notification.h
    OneG4/PluginInfo.h
    OneG4/RotatedWidget.h
    OneG4/Settings.h
    OneG4/Globals.h
    OneG4/XdgIcon.h
    OneG4/XdgDirs.h
)

add_library(oneg4-compat SHARED
    ${ONEG4_COMPAT_SOURCES}
    ${ONEG4_COMPAT_HEADERS}
)

target_include_directories(oneg4-compat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(oneg4-compat Qt6::Widgets Qt6::Gui Qt6::Core)
install(TARGETS oneg4-compat DESTINATION ${CMAKE_INSTALL_LIBDIR})




# Prevent in-source builds
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed.")
endif()

# Compiler settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wno-fuse-ld-path")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wno-fuse-ld-path")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wno-fuse-ld-path")
endif()

# Patch Version
set(ONEG4_PANEL_PATCH_VERSION 1)
set(ONEG4_PANEL_VERSION "1.0.0.${ONEG4_PANEL_PATCH_VERSION}")
add_definitions("-DONEG4_PANEL_VERSION=\"${ONEG4_PANEL_VERSION}\"")

# Warning: This must be before add_subdirectory(panel). Move with caution.
set(PLUGIN_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/1g4-panel")
add_definitions(
    -DPLUGIN_DIR=\"${PLUGIN_DIR}\"
)

message(STATUS "CMAKE Module linker flags: ${CMAKE_MODULE_LINKER_FLAGS}")
message(STATUS "Panel plugins location: ${PLUGIN_DIR}")

#########################################################################
# Plugin system
# You can enable/disable building of the plugin using cmake options.
#    cmake -DWORLDCLOCK_PLUGIN=Yes .. # Enable worldclock plugin
#    cmake -DWORLDCLOCK_PLUGIN=No ..  # Disable worldclock plugin

include("cmake/BuildPlugin.cmake")

set(ENABLED_PLUGINS) # list of enabled plugins
set(STATIC_PLUGINS) # list of statically linked plugins

setByDefault(COLORPICKER_PLUGIN No)
if(COLORPICKER_PLUGIN)
    list(APPEND ENABLED_PLUGINS "Color Picker")
    add_subdirectory(plugin-colorpicker)
endif()

setByDefault(CPULOAD_PLUGIN No)
if(CPULOAD_PLUGIN)
    find_library(STATGRAB_LIB statgrab)

    if(NOT STATGRAB_LIB)
        message(FATAL_ERROR "CPU Load plugin requires libstatgrab")
    endif()
    list(APPEND ENABLED_PLUGINS "Cpu Load")
    add_subdirectory(plugin-cpuload)
endif()

setByDefault(CUSTOMCOMMAND_PLUGIN No)
if(CUSTOMCOMMAND_PLUGIN)
    list(APPEND ENABLED_PLUGINS "Custom Command")
    add_subdirectory(plugin-customcommand)
endif()

setByDefault(DIRECTORYMENU_PLUGIN No)
if(DIRECTORYMENU_PLUGIN)
    list(APPEND ENABLED_PLUGINS "Directory menu")
    add_subdirectory(plugin-directorymenu)
endif()

setByDefault(DOM_PLUGIN No)
if(DOM_PLUGIN)
    list(APPEND ENABLED_PLUGINS "DOM")
    add_subdirectory(plugin-dom)
endif(DOM_PLUGIN)

setByDefault(DESKTOPSWITCH_PLUGIN No)
if(DESKTOPSWITCH_PLUGIN)
    list(APPEND STATIC_PLUGINS "desktopswitch")
    add_definitions(-DWITH_DESKTOPSWITCH_PLUGIN)
    list(APPEND ENABLED_PLUGINS "Desktop Switcher")
    add_subdirectory(plugin-desktopswitch)
endif()

setByDefault(FANCYMENU_PLUGIN No)
if(FANCYMENU_PLUGIN)
    list(APPEND STATIC_PLUGINS "fancymenu")
    add_definitions(-DWITH_FANCYMENU_PLUGIN)
    list(APPEND ENABLED_PLUGINS "Application fancy menu")
    add_subdirectory(plugin-fancymenu)
endif()

setByDefault(KBINDICATOR_PLUGIN No)
if(KBINDICATOR_PLUGIN)
    list(APPEND ENABLED_PLUGINS "Keyboard Indicator")
    add_subdirectory(plugin-kbindicator)
endif(KBINDICATOR_PLUGIN)

setByDefault(MAINMENU_PLUGIN No)
if(MAINMENU_PLUGIN)
    list(APPEND STATIC_PLUGINS "mainmenu")
    add_definitions(-DWITH_MAINMENU_PLUGIN)
    list(APPEND ENABLED_PLUGINS "Application menu")
    add_subdirectory(plugin-mainmenu)
endif()

setByDefault(QUICKLAUNCH_PLUGIN No)
if(QUICKLAUNCH_PLUGIN)
    list(APPEND STATIC_PLUGINS "quicklaunch")
    add_definitions(-DWITH_QUICKLAUNCH_PLUGIN)
    list(APPEND ENABLED_PLUGINS "Quicklaunch")
    add_subdirectory(plugin-quicklaunch)
endif()

setByDefault(SENSORS_PLUGIN No)
if(SENSORS_PLUGIN)
    find_library(SENSORS_LIB sensors)

    if(NOT SENSORS_LIB)
        message(FATAL_ERROR "Sensors plugin requires libsensors")
    endif()
    list(APPEND ENABLED_PLUGINS "Sensors")
    add_subdirectory(plugin-sensors)
endif()

setByDefault(SHOWDESKTOP_PLUGIN No)
if(SHOWDESKTOP_PLUGIN)
    list(APPEND STATIC_PLUGINS "showdesktop")
    add_definitions(-DWITH_SHOWDESKTOP_PLUGIN)
    list(APPEND ENABLED_PLUGINS "Show Desktop")
    add_subdirectory(plugin-showdesktop)
endif()

setByDefault(QEYES_PLUGIN No)
if(QEYES_PLUGIN)
    list(APPEND ENABLED_PLUGINS "QEyes")
    add_subdirectory(plugin-qeyes)
endif()


setByDefault(NETWORKMONITOR_PLUGIN No)
if(NETWORKMONITOR_PLUGIN)
    find_library(STATGRAB_LIB statgrab)

    if(NOT STATGRAB_LIB)
        message(FATAL_ERROR "Network Monitor plugin requires libstatgrab")
    endif()
    list(APPEND ENABLED_PLUGINS "Network Monitor")
    add_subdirectory(plugin-networkmonitor)
endif()

setByDefault(SYSSTAT_PLUGIN No)
if(SYSSTAT_PLUGIN)
    list(APPEND ENABLED_PLUGINS "System Stats")
    add_subdirectory(plugin-sysstat)
endif(SYSSTAT_PLUGIN)

setByDefault(TASKBAR_PLUGIN Yes)
if(TASKBAR_PLUGIN)
  list(APPEND STATIC_PLUGINS "taskbar")
  add_definitions(-DWITH_TASKBAR_PLUGIN)
  list(APPEND ENABLED_PLUGINS "Taskbar")
  add_subdirectory(plugin-taskbar)
endif()

setByDefault(STATUSNOTIFIER_PLUGIN Yes)
if(STATUSNOTIFIER_PLUGIN)
    list(APPEND STATIC_PLUGINS "statusnotifier")
    add_definitions(-DWITH_STATUSNOTIFIER_PLUGIN)
    list(APPEND ENABLED_PLUGINS "Status Notifier")
    add_subdirectory(plugin-statusnotifier)
endif()

setByDefault(VOLUME_PLUGIN Yes)
if(VOLUME_PLUGIN)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PIPEWIRE QUIET libpipewire-0.3)
    if(NOT PIPEWIRE_FOUND)
        message(FATAL_ERROR "Volume plugin requires PipeWire development files (pkg-config module: libpipewire-0.3)")
    endif()

    pkg_check_modules(WIREPLUMBER QUIET wireplumber-0.5)
    if(NOT WIREPLUMBER_FOUND)
        message(FATAL_ERROR "Volume plugin requires WirePlumber development files (pkg-config module: wireplumber-0.5)")
    endif()

    pkg_check_modules(BLUEZ QUIET bluez)
    if(NOT BLUEZ_FOUND)
        message(FATAL_ERROR "Volume plugin requires BlueZ development files (pkg-config module: bluez)")
    endif()

    list(APPEND ENABLED_PLUGINS   "Volume")
    message(STATUS "")
    message(STATUS "Volume plugin will be built")
    message(STATUS "    PipeWire: ${PIPEWIRE_VERSION}")
    message(STATUS "    WirePlumber: ${WIREPLUMBER_VERSION}")
    message(STATUS "    BlueZ: ${BLUEZ_VERSION}")
    message(STATUS "")
    add_subdirectory(plugin-volume)
endif()

setByDefault(WORLDCLOCK_PLUGIN Yes)
if(WORLDCLOCK_PLUGIN)
    list(APPEND STATIC_PLUGINS "worldclock")
    add_definitions(-DWITH_WORLDCLOCK_PLUGIN)
    list(APPEND ENABLED_PLUGINS "World Clock")
    add_subdirectory(plugin-worldclock)
endif(WORLDCLOCK_PLUGIN)

setByDefault(SPACER_PLUGIN Yes)
if(SPACER_PLUGIN)
    list(APPEND STATIC_PLUGINS "spacer")
    add_definitions(-DWITH_SPACER_PLUGIN)
    list(APPEND ENABLED_PLUGINS "Spacer")
    add_subdirectory(plugin-spacer)
endif()

setByDefault(BACKLIGHT_PLUGIN No)
if(BACKLIGHT_PLUGIN)
    list(APPEND ENABLED_PLUGINS "Backlight")
    add_subdirectory(plugin-backlight)
endif()

#########################################################################

message(STATUS  "**************** The following plugins will be built ****************")
foreach (PLUGIN_STR ${ENABLED_PLUGINS})
    message(STATUS "  ${PLUGIN_STR}")
endforeach()
message(STATUS "*********************************************************************")

add_subdirectory(panel)

# merged from oneg4-common
add_subdirectory(autostart)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
