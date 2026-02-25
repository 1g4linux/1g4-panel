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
        message(FATAL_ERROR "Missing mixer popup contract in header: ${POPUP_HEADER_CHECK}")
    endif()
endforeach()

foreach(POPUP_SOURCE_CHECK
        "m_externalMixerButton = new QPushButton(this);"
        "m_externalMixerButton->setObjectName(QStringLiteral(\"MixerLink\"));"
        "m_externalMixerButton->setToolTip(tr(\"Launch mixer\"));"
        "m_externalMixerButton->setIcon(XdgIcon::fromTheme(QLatin1String(\"audio-card\")));"
        "m_externalMixerButton->setAutoDefault(false);"
        "layout->addWidget(m_externalMixerButton, 0, Qt::AlignHCenter);"
        "connect(m_externalMixerButton, &QPushButton::clicked, this, &VolumePopup::handleExternalMixerClicked);"
        "void VolumePopup::handleExternalMixerClicked()"
        "emit externalMixerRequested();")
    string(FIND "${VOLUME_POPUP_SOURCE_CONTENT}" "${POPUP_SOURCE_CHECK}" POPUP_SOURCE_CHECK_POS)
    if(POPUP_SOURCE_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing mixer popup contract in source: ${POPUP_SOURCE_CHECK}")
    endif()
endforeach()

foreach(VOLUME_SOURCE_CHECK
        "connect(m_volumeButton->volumePopup(), &VolumePopup::externalMixerRequested, this, &OneG4Volume::openExternalMixer);"
        "void OneG4Volume::openExternalMixer()"
        "QDialog* create_1g4_mixer_dialog();"
        "m_mixerDialog = create_1g4_mixer_dialog();"
        "m_mixerDialog->show();"
        "m_mixerDialog->raise();")
    string(FIND "${ONEG4_VOLUME_SOURCE_CONTENT}" "${VOLUME_SOURCE_CHECK}" VOLUME_SOURCE_CHECK_POS)
    if(VOLUME_SOURCE_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing built-in mixer launch contract in oneg4volume.cpp: ${VOLUME_SOURCE_CHECK}")
    endif()
endforeach()

foreach(FORBIDDEN_VOLUME_SOURCE_CHECK
        "QStandardPaths::findExecutable("
        "QProcess::startDetached(")
    string(FIND "${ONEG4_VOLUME_SOURCE_CONTENT}" "${FORBIDDEN_VOLUME_SOURCE_CHECK}" FORBIDDEN_VOLUME_SOURCE_CHECK_POS)
    if(NOT FORBIDDEN_VOLUME_SOURCE_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "External mixer launcher contract should not be present: ${FORBIDDEN_VOLUME_SOURCE_CHECK}")
    endif()
endforeach()
