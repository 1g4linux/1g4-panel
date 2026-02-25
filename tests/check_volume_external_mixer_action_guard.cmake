if(NOT DEFINED ONEG4_VOLUME_SOURCE)
    message(FATAL_ERROR "ONEG4_VOLUME_SOURCE is required")
endif()
if(NOT DEFINED VOLUME_POPUP_HEADER)
    message(FATAL_ERROR "VOLUME_POPUP_HEADER is required")
endif()
if(NOT DEFINED VOLUME_POPUP_SOURCE)
    message(FATAL_ERROR "VOLUME_POPUP_SOURCE is required")
endif()

file(READ "${ONEG4_VOLUME_SOURCE}" ONEG4_VOLUME_SOURCE_CONTENT)
file(READ "${VOLUME_POPUP_HEADER}" VOLUME_POPUP_HEADER_CONTENT)
file(READ "${VOLUME_POPUP_SOURCE}" VOLUME_POPUP_SOURCE_CONTENT)

foreach(POPUP_HEADER_CHECK
        "void externalMixerRequested();"
        "void handleExternalMixerClicked();")
    string(FIND "${VOLUME_POPUP_HEADER_CONTENT}" "${POPUP_HEADER_CHECK}" POPUP_HEADER_CHECK_POS)
    if(POPUP_HEADER_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing external mixer popup contract in header: ${POPUP_HEADER_CHECK}")
    endif()
endforeach()

foreach(POPUP_SOURCE_CHECK
        "m_externalMixerButton = new QPushButton(tr(\"Mixer\"), this);"
        "connect(m_externalMixerButton, &QPushButton::clicked, this, &VolumePopup::handleExternalMixerClicked);"
        "void VolumePopup::handleExternalMixerClicked()"
        "emit externalMixerRequested();")
    string(FIND "${VOLUME_POPUP_SOURCE_CONTENT}" "${POPUP_SOURCE_CHECK}" POPUP_SOURCE_CHECK_POS)
    if(POPUP_SOURCE_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing external mixer popup contract in source: ${POPUP_SOURCE_CHECK}")
    endif()
endforeach()

foreach(VOLUME_SOURCE_CHECK
        "connect(m_volumeButton->volumePopup(), &VolumePopup::externalMixerRequested, this, &OneG4Volume::openExternalMixer);"
        "void OneG4Volume::openExternalMixer()"
        "QStandardPaths::findExecutable(candidate)"
        "QProcess::startDetached(executable, {})")
    string(FIND "${ONEG4_VOLUME_SOURCE_CONTENT}" "${VOLUME_SOURCE_CHECK}" VOLUME_SOURCE_CHECK_POS)
    if(VOLUME_SOURCE_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing external mixer launch contract in oneg4volume.cpp: ${VOLUME_SOURCE_CHECK}")
    endif()
endforeach()
