if(NOT DEFINED VOLUME_DESIGN_DECISIONS_DOC)
    message(FATAL_ERROR "VOLUME_DESIGN_DECISIONS_DOC is required")
endif()

file(READ "${VOLUME_DESIGN_DECISIONS_DOC}" VOLUME_DESIGN_DECISIONS_DOC_CONTENT)

foreach(REQUIRED_SNIPPET
        "## Decision 1: Control path architecture"
        "Choice: Hybrid PipeWire + WirePlumber path."
        "## Decision 2: WirePlumber integration depth"
        "Choice: Hard build dependency for backend stack validation, optional policy UI integration at runtime."
        "## Decision 3: Interaction surface"
        "Choice: Keep compact popover for common controls and use plugin configuration dialog for advanced/policy controls."
        "## Decision 4: Bluetooth mic auto-switch default"
        "Choice: Manual profile switching by default; no implicit auto-switch until policy heuristics are validated."
        "## Decision 5: `pavucontrol-qt` parity split"
        "v1: core panel controls"
        "v2: deeper per-stream routing/profile controls"
        "## Decision 6: Battery indicator placement"
        "Choice: Show battery in popup/tooltip context, not as a persistent panel icon overlay."
        "## Decision 7: Per-device policy storage and migration"
        "Choice: Persist policy in WirePlumber SPA-JSON")
    string(FIND "${VOLUME_DESIGN_DECISIONS_DOC_CONTENT}" "${REQUIRED_SNIPPET}" REQUIRED_SNIPPET_POS)
    if(REQUIRED_SNIPPET_POS EQUAL -1)
        message(FATAL_ERROR "Missing design decision documentation snippet: ${REQUIRED_SNIPPET}")
    endif()
endforeach()
