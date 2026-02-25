set(PLUGIN "volume")

set(HEADERS
    oneg4volume.h
    volumebutton.h
    volumepopup.h
    audiodevice.h
    oneg4volumeconfiguration.h
    audioengine.h
    wireplumberpolicy.h
    sinkselection.h
    volumelogging.h
)


set(SOURCES
    oneg4volume.cpp
    volumebutton.cpp
    volumepopup.cpp
    audiodevice.cpp
    oneg4volumeconfiguration.cpp
    audioengine.cpp
    wireplumberpolicy.cpp
    sinkselection.cpp
    volumelogging.cpp
)


set(UIS
    oneg4volumeconfiguration.ui
)


set(LIBRARIES ${LIBRARIES})

# Audio backend selection
if(NOT PIPEWIRE_FOUND)
  message(FATAL_ERROR "Volume plugin requires top-level PipeWire detection (PIPEWIRE_FOUND)")
endif()
add_definitions(-DUSE_PIPEWIRE)
include_directories(SYSTEM ${PIPEWIRE_INCLUDE_DIRS})
set(LIBRARIES ${LIBRARIES} ${PIPEWIRE_LIBRARIES})
message(STATUS "Volume plugin: PipeWire backend available")

set(SOURCES ${SOURCES} pipewireengine.cpp)
set(HEADERS ${HEADERS} pipewireengine.h)
set(MOCS ${MOCS} pipewireengine.h)

if(ONEG4_VOLUME_DEV_TEST_BACKENDS)
  set(SOURCES ${SOURCES} testaudioengine.cpp)
  set(HEADERS ${HEADERS} testaudioengine.h)
  set(MOCS ${MOCS} testaudioengine.h)
endif()

BUILD_ONEG4_PLUGIN(${PLUGIN})
if(ONEG4_VOLUME_DEV_VERBOSE_LOGGING)
  target_compile_definitions(${PLUGIN} PRIVATE ONEG4_VOLUME_DEV_VERBOSE_LOGGING=1)
endif()
if(ONEG4_VOLUME_DEV_TEST_BACKENDS)
  target_compile_definitions(${PLUGIN} PRIVATE ONEG4_VOLUME_DEV_TEST_BACKENDS=1)
endif()
if(ONEG4_VOLUME_ENABLE_WIREPLUMBER_POLICY)
  target_compile_definitions(${PLUGIN} PRIVATE ONEG4_VOLUME_ENABLE_WIREPLUMBER_POLICY=1)
endif()
if(ONEG4_VOLUME_ENABLE_BLUETOOTH_BATTERY)
  target_compile_definitions(${PLUGIN} PRIVATE ONEG4_VOLUME_ENABLE_BLUETOOTH_BATTERY=1)
endif()
