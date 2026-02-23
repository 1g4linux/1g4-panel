find_package(Qt6Test REQUIRED)

add_executable(plugin_volume_sinkselection_test
    sinkselection_test.cpp
    ${CMAKE_SOURCE_DIR}/plugin-volume/sinkselection.cpp
)
target_include_directories(plugin_volume_sinkselection_test PRIVATE ${CMAKE_SOURCE_DIR}/plugin-volume)
target_link_libraries(plugin_volume_sinkselection_test PRIVATE Qt6::Core Qt6::Test)

add_test(
    NAME plugin_volume_sinkselection_test
    COMMAND plugin_volume_sinkselection_test
)

add_executable(plugin_volume_audioengine_ignoremaxvolume_test
    audioengine_ignoremaxvolume_test.cpp
    ${CMAKE_SOURCE_DIR}/plugin-volume/audioengine.cpp
    ${CMAKE_SOURCE_DIR}/plugin-volume/audiodevice.cpp
)
target_include_directories(plugin_volume_audioengine_ignoremaxvolume_test PRIVATE ${CMAKE_SOURCE_DIR}/plugin-volume)
target_link_libraries(plugin_volume_audioengine_ignoremaxvolume_test PRIVATE Qt6::Core Qt6::Test)

add_test(
    NAME plugin_volume_audioengine_ignoremaxvolume_test
    COMMAND plugin_volume_audioengine_ignoremaxvolume_test
)

add_test(
    NAME plugin_volume_preprocessor_fallback_guard
    COMMAND ${CMAKE_COMMAND}
            -DONEG4_VOLUME_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volume.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_oneg4volume_preprocessor_guard.cmake
)

add_test(
    NAME plugin_volume_pulseaudio_unique_connection_guard
    COMMAND ${CMAKE_COMMAND}
            -DPULSEAUDIO_ENGINE_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/pulseaudioengine.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_pulseaudio_unique_connection_guard.cmake
)

add_test(
    NAME plugin_volume_keyboard_notification_setting_removed_guard
    COMMAND ${CMAKE_COMMAND}
            -DONEG4_VOLUME_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volume.cpp
            -DONEG4_VOLUME_CONFIGURATION_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volumeconfiguration.cpp
            -DONEG4_VOLUME_CONFIGURATION_HEADER=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volumeconfiguration.h
            -DONEG4_VOLUME_CONFIGURATION_UI=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volumeconfiguration.ui
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_keyboard_notification_setting_removed.cmake
)

add_test(
    NAME plugin_volume_pipewire_debug_logging_guard
    COMMAND ${CMAKE_COMMAND}
            -DPIPEWIRE_ENGINE_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/pipewireengine.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_pipewire_debug_logging_guard.cmake
)
