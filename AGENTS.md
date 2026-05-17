# AGENTS.md

## Project Summary

`qsensors` is a native Qt6 Widgets application for Linux/Wayland that displays hardware sensor data directly from `libsensors` (lm-sensors), inspired by `xsensors` in look and behavior.

Primary focus:
- reliable direct sensor integration (`libsensors`)
- responsive, readable UI
- stable window/layout behavior across restarts

## Scope and Constraints

- Language: C++20
- UI framework: Qt6 Widgets
- Build system: CMake
- Sensor access: `libsensors` (`libsensors4` / `libsensors-dev`)
- Primary runtime target: Linux/Wayland
- License: GPL-2.0-or-later

## Non-Goals

- CLI parsing as primary sensor source (`sensors` command output)
- mandatory X11-specific UI path
- QML
- hardware-in-the-loop test requirements in CI
- pixel/screenshot-based UI regression tests
- major architecture rewrites without prior alignment
- packaging work beyond currently requested targets
- silent settings-schema/key migrations

## Current Status

- Qt6 Widgets app builds and runs reliably
- sensor data is read directly via `libsensors`
- polling is configurable (`1..10s`, default `2s`)
- UI is xsensors-oriented with per-chip grouping and per-sensor cards
- LCD-style value rendering via theme glyph atlas
- alarm visualization integrated into LCD rendering
- chip sections are individually collapsible
- settings are integrated as a panel in the main view
- persistent UI/runtime state via `QSettings`:
  - window geometry
  - chip expand/collapse state
  - polling interval
  - default fan max RPM fallback
- structural vs value updates are separated to keep refreshes efficient
- minimum-width behavior is stabilized to avoid layout jumps

## Architecture (High-Level)

### Sensors Backend

Responsible for:
- `libsensors` init/cleanup lifecycle
- chip/feature/subfeature enumeration
- normalized `SensorReading` output
- runtime config application for backend policy values

### UI Composition

- `MainWindow`: top-level orchestration (polling, persistence, sizing behavior)
- `SensorsPanel`: chip/category layout and widget reconciliation
- `SettingsPanel`: runtime options (polling interval, fan fallback max RPM)
- `SensorValueWidget`: per-sensor card (title, LCD value, range bar)
- `LcdDisplayWidget` + `LcdGlyphAtlas`: atlas-based LCD rendering

### Data/State Flow
1. timer triggers backend poll
2. readings are normalized and delivered to panel layer
3. structure changes trigger selective layout rebuild
4. value-only changes update existing widgets in place
5. relevant UI/runtime state is persisted via `QSettings`

## Product and UX Decisions

- UI technology: Qt Widgets
- default behavior prioritizes visual stability (no width “jumping”)
- localized UI baseline: `en`, `de`, `fr`, `es`
- English source text is canonical for translation workflow

## Quality Requirements

- no lifecycle leaks around `sensors_init` / `sensors_cleanup`
- robust behavior if sensors configuration is missing/invalid
- responsive UI under periodic polling
- explicit, visible error reporting (no silent failure)
- avoid overwriting unrelated user/worktree changes

## Build and Test Commands (Agent Quick Use)

- Configure: `cmake -S . -B build -DBUILD_TESTING=ON`
- Build: `cmake --build build -j`
- Run tests: `ctest --test-dir build --output-on-failure`

## Working Guidelines (Agent Instructions)

- prefer small, isolated commits per change package
- align briefly before larger refactors
- run build/tests/checks after functional changes
- treat failing tests as blockers; do not consider work complete until tests are green
- any behavior change should add or update automated tests when feasible
- keep data model and presentation concerns clearly separated
- translation drift is CI-gated: translation source files must stay in sync (run `cmake --build build --target update_translations`)

## AGENTS.md Governance

- keep this file current: if architecture, workflow, quality gates, or project policies change, update `AGENTS.md` in the same change package.
- before changing principles or policies documented in `AGENTS.md`, explicitly align with the project owner first.

## Translation Policy

Project policy: any new or changed user-visible string should be reflected in the translation workflow for supported locales (`en`, `de`, `fr`, `es`) before considering the change complete.
