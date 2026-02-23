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
)


set(UIS
    oneg4volumeconfiguration.ui
)


set(LIBRARIES ${LIBRARIES})

# Audio backend selection
find_package(PkgConfig REQUIRED)

# PipeWire backend
pkg_check_modules(PIPEWIRE libpipewire-0.3)
if(PIPEWIRE_FOUND)
  add_definitions(-DUSE_PIPEWIRE)
  include_directories(SYSTEM ${PIPEWIRE_INCLUDE_DIRS})
  set(LIBRARIES ${LIBRARIES} ${PIPEWIRE_LIBRARIES})
  message(STATUS "Volume plugin: PipeWire backend available")
else()
  message(STATUS "Volume plugin: PipeWire backend not available")
endif()

# PulseAudio backend
pkg_check_modules(PULSEAUDIO libpulse)
if(PULSEAUDIO_FOUND)
  add_definitions(-DUSE_PULSEAUDIO)
  include_directories(SYSTEM ${PULSEAUDIO_INCLUDE_DIRS})
  set(LIBRARIES ${LIBRARIES} ${PULSEAUDIO_LIBRARIES})
  message(STATUS "Volume plugin: PulseAudio backend available")
else()
  message(STATUS "Volume plugin: PulseAudio backend not available")
endif()

# Ensure at least one backend is available
if(NOT PIPEWIRE_FOUND AND NOT PULSEAUDIO_FOUND)
  message(FATAL_ERROR "Volume plugin: No audio backend found. Need at least PipeWire or PulseAudio.")
endif()

# Conditionally add engine sources and headers
if(PIPEWIRE_FOUND)
  set(SOURCES ${SOURCES} pipewireengine.cpp)
  set(HEADERS ${HEADERS} pipewireengine.h)
  set(MOCS ${MOCS} pipewireengine.h)
endif()

if(PULSEAUDIO_FOUND)
  set(SOURCES ${SOURCES} pulseaudioengine.cpp)
  set(HEADERS ${HEADERS} pulseaudioengine.h)
  set(MOCS ${MOCS} pulseaudioengine.h)
endif()

add_subdirectory(1g4-mixer)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/1g4-mixer/src)


set(LIBRARIES ${LIBRARIES} 1g4-mixer)

BUILD_ONEG4_PLUGIN(${PLUGIN})
target_compile_definitions(${PLUGIN} PRIVATE ONEG4_MIXER_LIBRARY=1)
