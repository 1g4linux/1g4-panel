# Volume Plugin Design Decisions

This document resolves the initial architecture decisions listed in `TODO.md`.

## Decision 1: Control path architecture

- Choice: Hybrid PipeWire + WirePlumber path.
- Rationale:
  - Use PipeWire-native object state/event handling for runtime discovery and control.
  - Use WirePlumber for policy persistence and policy-aware operations.

## Decision 2: WirePlumber integration depth

- Choice: Hard build dependency for backend stack validation, optional policy UI integration at runtime.
- Rationale:
  - Build should fail fast when WirePlumber development headers are not present.
  - Advanced policy controls remain gated by feature toggles and capability checks.

## Decision 3: Interaction surface

- Choice: Keep compact popover for common controls and use plugin configuration dialog for advanced/policy controls.
- Rationale:
  - Common actions stay fast from the panel.
  - Advanced controls avoid overloading the popover.

## Decision 4: Bluetooth mic auto-switch default

- Choice: Manual profile switching by default; no implicit auto-switch until policy heuristics are validated.
- Rationale:
  - Prevent profile thrash and unexpected quality downgrades.
  - Keep behavior predictable under reconnect storms.

## Decision 5: `pavucontrol-qt` parity split

- Choice:
  - v1: core panel controls (volume/mute/default endpoint selection and backend availability reporting).
  - v2: deeper per-stream routing/profile controls and richer Bluetooth policy automation.
- Rationale:
  - De-risks migration while preserving a clear target path.

## Decision 6: Battery indicator placement

- Choice: Show battery in popup/tooltip context, not as a persistent panel icon overlay.
- Rationale:
  - Avoid icon clutter while keeping status discoverable.

## Decision 7: Per-device policy storage and migration

- Choice: Persist policy in WirePlumber SPA-JSON (`~/.config/wireplumber/wireplumber.conf.d/60-1g4-panel-volume.conf`) with stable device identifiers.
- Rationale:
  - Aligns with WirePlumber-native policy flow.
  - Supports deterministic migration from transient runtime IDs.
