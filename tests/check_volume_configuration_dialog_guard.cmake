if(NOT DEFINED ONEG4_VOLUME_HEADER)
    message(FATAL_ERROR "ONEG4_VOLUME_HEADER is required")
endif()
if(NOT DEFINED ONEG4_VOLUME_SOURCE)
    message(FATAL_ERROR "ONEG4_VOLUME_SOURCE is required")
endif()
if(NOT DEFINED ONEG4_VOLUME_CONFIGURATION_UI)
    message(FATAL_ERROR "ONEG4_VOLUME_CONFIGURATION_UI is required")
endif()
if(NOT DEFINED VOLUME_BUTTON_SOURCE)
    message(FATAL_ERROR "VOLUME_BUTTON_SOURCE is required")
endif()
if(NOT DEFINED VOLUME_POPUP_SOURCE)
    message(FATAL_ERROR "VOLUME_POPUP_SOURCE is required")
endif()
if(NOT DEFINED VOLUME_PLUGIN_DESKTOP)
    message(FATAL_ERROR "VOLUME_PLUGIN_DESKTOP is required")
endif()

file(READ "${ONEG4_VOLUME_HEADER}" ONEG4_VOLUME_HEADER_CONTENT)
file(READ "${ONEG4_VOLUME_SOURCE}" ONEG4_VOLUME_SOURCE_CONTENT)
file(READ "${ONEG4_VOLUME_CONFIGURATION_UI}" ONEG4_VOLUME_CONFIGURATION_UI_CONTENT)
file(READ "${VOLUME_BUTTON_SOURCE}" VOLUME_BUTTON_SOURCE_CONTENT)
file(READ "${VOLUME_POPUP_SOURCE}" VOLUME_POPUP_SOURCE_CONTENT)
file(READ "${VOLUME_PLUGIN_DESKTOP}" VOLUME_PLUGIN_DESKTOP_CONTENT)

foreach(HEADER_CHECK
        "Q_PLUGIN_METADATA(IID \"oneg4.org/Panel/PluginInterface/3.0\")"
        "virtual IOneG4PanelPlugin::Flags flags() const { return PreferRightAlignment | HaveConfigDialog; }"
        "QDialog* configureDialog();")
    string(FIND "${ONEG4_VOLUME_HEADER_CONTENT}" "${HEADER_CHECK}" HEADER_CHECK_POS)
    if(HEADER_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing volume plugin configuration dialog header contract: ${HEADER_CHECK}")
    endif()
endforeach()

foreach(SOURCE_CHECK
        "QDialog* OneG4Volume::configureDialog()"
        "m_configDialog = new OneG4VolumeConfiguration(settings(), false);"
        "m_configDialog->setAttribute(Qt::WA_DeleteOnClose, true);"
        "connect(m_configDialog, &QObject::destroyed, this, [this] { m_configDialog = nullptr; });"
        "return m_configDialog;")
    string(FIND "${ONEG4_VOLUME_SOURCE_CONTENT}" "${SOURCE_CHECK}" SOURCE_CHECK_POS)
    if(SOURCE_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing volume plugin configuration dialog source contract: ${SOURCE_CHECK}")
    endif()
endforeach()

foreach(UI_CHECK
        "<widget class=\"QDialog\" name=\"OneG4VolumeConfiguration\">"
        "<widget class=\"QComboBox\" name=\"audioBackendCombo\"/>"
        "<widget class=\"QComboBox\" name=\"devAddedCombo\"/>"
        "<widget class=\"QDialogButtonBox\" name=\"buttons\">")
    string(FIND "${ONEG4_VOLUME_CONFIGURATION_UI_CONTENT}" "${UI_CHECK}" UI_CHECK_POS)
    if(UI_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing volume configuration UI skeleton element: ${UI_CHECK}")
    endif()
endforeach()

foreach(BUTTON_CHECK
        "handleStockIconChanged(QStringLiteral(\"dialog-error\"));"
        "connect(m_volumePopup, &VolumePopup::stockIconChanged, this, &VolumeButton::handleStockIconChanged);")
    string(FIND "${VOLUME_BUTTON_SOURCE_CONTENT}" "${BUTTON_CHECK}" BUTTON_CHECK_POS)
    if(BUTTON_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing volume button icon contract: ${BUTTON_CHECK}")
    endif()
endforeach()

foreach(POPUP_CHECK
        "m_muteToggleButton->setIcon(XdgIcon::fromTheme(QLatin1String(\"audio-volume-muted-panel\")));"
        "iconName = QLatin1String(\"audio-volume-high\");"
        "iconName.append(QLatin1String(\"-panel\"));"
        "emit stockIconChanged(iconName);")
    string(FIND "${VOLUME_POPUP_SOURCE_CONTENT}" "${POPUP_CHECK}" POPUP_CHECK_POS)
    if(POPUP_CHECK_POS EQUAL -1)
        message(FATAL_ERROR "Missing volume popup icon contract: ${POPUP_CHECK}")
    endif()
endforeach()

string(FIND "${VOLUME_PLUGIN_DESKTOP_CONTENT}" "Icon=multimedia-volume-control" DESKTOP_ICON_POS)
if(DESKTOP_ICON_POS EQUAL -1)
    message(FATAL_ERROR "Volume plugin desktop metadata is missing its baseline icon declaration")
endif()
