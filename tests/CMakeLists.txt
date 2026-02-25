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

add_executable(plugin_volume_audioengine_snapshot_test
    audioengine_snapshot_test.cpp
    ${CMAKE_SOURCE_DIR}/plugin-volume/audioengine.cpp
    ${CMAKE_SOURCE_DIR}/plugin-volume/audiodevice.cpp
)
target_include_directories(plugin_volume_audioengine_snapshot_test PRIVATE ${CMAKE_SOURCE_DIR}/plugin-volume)
target_link_libraries(plugin_volume_audioengine_snapshot_test PRIVATE Qt6::Core Qt6::Test)

add_test(
    NAME plugin_volume_audioengine_snapshot_test
    COMMAND plugin_volume_audioengine_snapshot_test
)

add_executable(plugin_volume_volumepopup_drag_test
    volumepopup_drag_test.cpp
    ${CMAKE_SOURCE_DIR}/plugin-volume/volumepopup.cpp
    ${CMAKE_SOURCE_DIR}/plugin-volume/audioengine.cpp
    ${CMAKE_SOURCE_DIR}/plugin-volume/audiodevice.cpp
    ${CMAKE_SOURCE_DIR}/OneG4/XdgIcon.cpp
)
target_include_directories(plugin_volume_volumepopup_drag_test PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/plugin-volume)
target_link_libraries(plugin_volume_volumepopup_drag_test PRIVATE Qt6::Core Qt6::Test Qt6::Widgets)

add_test(
    NAME plugin_volume_volumepopup_drag_test
    COMMAND ${CMAKE_COMMAND} -E env
            QT_QPA_PLATFORM=offscreen
            QT_QPA_PLATFORMTHEME=
            QT_STYLE_OVERRIDE=Fusion
            XDG_CURRENT_DESKTOP=
            $<TARGET_FILE:plugin_volume_volumepopup_drag_test>
)

add_executable(plugin_volume_configuration_lazy_rebuild_test
    volumeconfiguration_lazy_rebuild_test.cpp
    ${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volumeconfiguration.cpp
    ${CMAKE_SOURCE_DIR}/plugin-volume/sinkselection.cpp
    ${CMAKE_SOURCE_DIR}/plugin-volume/wireplumberpolicy.cpp
    ${CMAKE_SOURCE_DIR}/plugin-volume/volumelogging.cpp
    ${CMAKE_SOURCE_DIR}/plugin-volume/audioengine.cpp
    ${CMAKE_SOURCE_DIR}/plugin-volume/audiodevice.cpp
    ${CMAKE_SOURCE_DIR}/panel/pluginsettings.cpp
    ${CMAKE_SOURCE_DIR}/panel/oneg4panelpluginconfigdialog.cpp
    ${CMAKE_SOURCE_DIR}/OneG4/Settings.cpp
)
target_include_directories(plugin_volume_configuration_lazy_rebuild_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/plugin-volume
        ${CMAKE_SOURCE_DIR}/panel
)
target_link_libraries(plugin_volume_configuration_lazy_rebuild_test PRIVATE Qt6::Core Qt6::Test Qt6::Widgets oneg4-compat)

add_test(
    NAME plugin_volume_configuration_lazy_rebuild_test
    COMMAND ${CMAKE_COMMAND} -E env
            QT_QPA_PLATFORM=offscreen
            QT_QPA_PLATFORMTHEME=
            QT_STYLE_OVERRIDE=Fusion
            XDG_CURRENT_DESKTOP=
            $<TARGET_FILE:plugin_volume_configuration_lazy_rebuild_test>
)

add_executable(plugin_volume_plugin_load_test
    plugin_volume_plugin_load_test.cpp
    ${CMAKE_SOURCE_DIR}/panel/pluginsettings.cpp
    ${CMAKE_SOURCE_DIR}/panel/oneg4panelpluginconfigdialog.cpp
)
target_include_directories(plugin_volume_plugin_load_test PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/panel)
target_link_libraries(plugin_volume_plugin_load_test PRIVATE Qt6::Core Qt6::Test Qt6::Widgets oneg4-compat)
set_target_properties(plugin_volume_plugin_load_test PROPERTIES ENABLE_EXPORTS ON)

add_test(
    NAME plugin_volume_plugin_load_test
    COMMAND ${CMAKE_COMMAND} -E env
            ONEG4_VOLUME_PLUGIN_PATH=$<TARGET_FILE:volume>
            ONEG4_VOLUME_DESKTOP_PATH=${CMAKE_SOURCE_DIR}/plugin-volume/resources/volume.desktop.in
            QT_QPA_PLATFORMTHEME=
            QT_STYLE_OVERRIDE=Fusion
            XDG_CURRENT_DESKTOP=
            $<TARGET_FILE:plugin_volume_plugin_load_test>
)

add_test(
    NAME plugin_volume_preprocessor_fallback_guard
    COMMAND ${CMAKE_COMMAND}
            -DONEG4_VOLUME_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volume.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_oneg4volume_preprocessor_guard.cmake
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

add_test(
    NAME plugin_volume_pipewire_teardown_queued_signal_guard
    COMMAND ${CMAKE_COMMAND}
            -DPIPEWIRE_ENGINE_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/pipewireengine.cpp
            -DPIPEWIRE_ENGINE_HEADER=${CMAKE_SOURCE_DIR}/plugin-volume/pipewireengine.h
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_pipewire_teardown_queued_signal_guard.cmake
)

add_test(
    NAME plugin_volume_dependency_checks_guard
    COMMAND ${CMAKE_COMMAND}
            -DONEG4_PANEL_CMAKE=${CMAKE_SOURCE_DIR}/CMakeLists.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_volume_dependency_checks_guard.cmake
)

add_test(
    NAME plugin_volume_build_integration_guard
    COMMAND ${CMAKE_COMMAND}
            -DONEG4_PANEL_CMAKE=${CMAKE_SOURCE_DIR}/CMakeLists.txt
            -DVOLUME_PLUGIN_CMAKE=${CMAKE_SOURCE_DIR}/plugin-volume/CMakeLists.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_volume_build_integration_guard.cmake
)

add_test(
    NAME plugin_volume_standalone_dev_path_guard
    COMMAND ${CMAKE_COMMAND}
            -DONEG4_PANEL_CMAKE=${CMAKE_SOURCE_DIR}/CMakeLists.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_volume_standalone_dev_path_guard.cmake
)

add_test(
    NAME plugin_volume_dev_flags_guard
    COMMAND ${CMAKE_COMMAND}
            -DONEG4_PANEL_CMAKE=${CMAKE_SOURCE_DIR}/CMakeLists.txt
            -DVOLUME_PLUGIN_CMAKE=${CMAKE_SOURCE_DIR}/plugin-volume/CMakeLists.txt
            -DONEG4_VOLUME_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volume.cpp
            -DONEG4_VOLUME_CONFIGURATION_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volumeconfiguration.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_volume_dev_flags_guard.cmake
)

add_test(
    NAME plugin_volume_feature_toggles_guard
    COMMAND ${CMAKE_COMMAND}
            -DONEG4_PANEL_CMAKE=${CMAKE_SOURCE_DIR}/CMakeLists.txt
            -DVOLUME_PLUGIN_CMAKE=${CMAKE_SOURCE_DIR}/plugin-volume/CMakeLists.txt
            -DONEG4_VOLUME_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volume.cpp
            -DONEG4_VOLUME_CONFIGURATION_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volumeconfiguration.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_volume_feature_toggles_guard.cmake
)

add_test(
    NAME plugin_volume_logging_categories_guard
    COMMAND ${CMAKE_COMMAND}
            -DONEG4_VOLUME_LOGGING_HEADER=${CMAKE_SOURCE_DIR}/plugin-volume/volumelogging.h
            -DONEG4_VOLUME_LOGGING_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/volumelogging.cpp
            -DONEG4_VOLUME_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volume.cpp
            -DPIPEWIRE_ENGINE_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/pipewireengine.cpp
            -DWIREPLUMBER_POLICY_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/wireplumberpolicy.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_volume_logging_categories_guard.cmake
)

add_test(
    NAME plugin_volume_configuration_dialog_guard
    COMMAND ${CMAKE_COMMAND}
            -DONEG4_VOLUME_HEADER=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volume.h
            -DONEG4_VOLUME_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volume.cpp
            -DONEG4_VOLUME_CONFIGURATION_UI=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volumeconfiguration.ui
            -DVOLUME_BUTTON_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/volumebutton.cpp
            -DVOLUME_POPUP_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/volumepopup.cpp
            -DVOLUME_PLUGIN_DESKTOP=${CMAKE_SOURCE_DIR}/plugin-volume/resources/volume.desktop.in
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_volume_configuration_dialog_guard.cmake
)

add_test(
    NAME plugin_volume_external_mixer_action_guard
    COMMAND ${CMAKE_COMMAND}
            -DONEG4_VOLUME_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volume.cpp
            -DVOLUME_POPUP_HEADER=${CMAKE_SOURCE_DIR}/plugin-volume/volumepopup.h
            -DVOLUME_POPUP_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/volumepopup.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_volume_external_mixer_action_guard.cmake
)

add_test(
    NAME plugin_volume_backend_ui_queued_connections_guard
    COMMAND ${CMAKE_COMMAND}
            -DONEG4_VOLUME_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volume.cpp
            -DVOLUME_POPUP_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/volumepopup.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_volume_backend_ui_queued_connections_guard.cmake
)

add_test(
    NAME plugin_volume_audio_device_parent_ownership_guard
    COMMAND ${CMAKE_COMMAND}
            -DPIPEWIRE_ENGINE_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/pipewireengine.cpp
            -DPULSEAUDIO_ENGINE_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/pulseaudioengine.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_audio_device_parent_ownership_guard.cmake
)

add_test(
    NAME plugin_volume_pipewire_only_backend_guard
    COMMAND ${CMAKE_COMMAND}
            -DVOLUME_PLUGIN_CMAKE=${CMAKE_SOURCE_DIR}/plugin-volume/CMakeLists.txt
            -DONEG4_VOLUME_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volume.cpp
            -DONEG4_VOLUME_HEADER=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volume.h
            -DONEG4_VOLUME_CONFIGURATION_SOURCE=${CMAKE_SOURCE_DIR}/plugin-volume/oneg4volumeconfiguration.cpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_volume_pipewire_only_backend_guard.cmake
)

add_test(
    NAME plugin_volume_no_libpulse_runtime_guard
    COMMAND ${CMAKE_COMMAND}
            -DVOLUME_PLUGIN_SO=$<TARGET_FILE:volume>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_volume_no_libpulse_runtime_guard.cmake
)

add_test(
    NAME plugin_volume_backend_target_matrix_doc_guard
    COMMAND ${CMAKE_COMMAND}
            -DVOLUME_BACKEND_TARGET_DOC=${CMAKE_SOURCE_DIR}/plugin-volume/docs/backend-target-matrix.md
            -DONEG4_PANEL_CMAKE=${CMAKE_SOURCE_DIR}/CMakeLists.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_volume_backend_target_matrix_doc_guard.cmake
)

add_test(
    NAME plugin_volume_design_decisions_doc_guard
    COMMAND ${CMAKE_COMMAND}
            -DVOLUME_DESIGN_DECISIONS_DOC=${CMAKE_SOURCE_DIR}/plugin-volume/docs/design-decisions.md
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_volume_design_decisions_doc_guard.cmake
)
