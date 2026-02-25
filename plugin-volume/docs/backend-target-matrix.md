# Volume Backend Target Matrix

This document defines the control-stack target for the volume plugin rewrite.

## Target backend

- Primary control path: PipeWire + WirePlumber + BlueZ.
- Minimum supported PipeWire version: 0.3.0.
- Minimum supported WirePlumber version: 0.5.0.
- Minimum supported BlueZ version: 5.0.

## Build-time enforcement

- Top-level CMake checks for `libpipewire-0.3`, `wireplumber-0.5`, and `bluez`.
- The checks enforce the minimum versions listed above and fail configuration with explicit errors.

## Non-goals

- No PulseAudio backend in the final control path.
- No libpulse control implementation in the final plugin backend.
- No PulseAudio-specific UX terminology where generic audio terms are sufficient.

## Capability detection behavior contract

- Missing PipeWire runtime connection:
  - Keep plugin loaded.
  - Surface backend-unavailable status in icon/tooltip.
  - Disable interactive controls that require a live backend.
- Missing WirePlumber policy capability:
  - Hide or disable policy-specific controls.
  - Log that policy integration is unavailable.
- Missing BlueZ battery/profile/port properties:
  - Keep core volume/mute/default routing controls active.
  - Show capability fields as unavailable rather than silently omitting state transitions.

## Status

- The plugin build/runtime path is restricted to PipeWire + WirePlumber + BlueZ, with no PulseAudio/libpulse control dependency.
